/*
 * Glyph C6 Monitor - Zigbee Core Module
 * 
 * Version: 1.0.0
 * 
 * Handles Zigbee stack initialization, cluster creation, device setup,
 * network management, and main loop processing.
 */

#include "zigbee_core.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_zigbee_attribute.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Define missing Power Config cluster attribute IDs (not in ESP Zigbee SDK headers)
#ifndef ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID
#define ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID        0x0021
#endif

#ifndef ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID
#define ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID                     0x0020
#endif

static const char *TAG = "ZIGBEE_CORE";

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================

// Zigbee device information
static zigbee_device_info_t device_info = {
    .zigbee_joined = false,
    .last_zigbee_report = 0,
    .pan_id = 0,
    .channel = 0,
    .short_address = 0
};

// Task handles
static TaskHandle_t zigbee_main_loop_task_handle = NULL;

// Action handler callback
static esp_err_t (*action_handler_callback)(esp_zb_core_action_callback_id_t, const void *) = NULL;

// ============================================================================
// PRIVATE FUNCTION PROTOTYPES
// ============================================================================

static void zigbee_main_loop_task(void *param);
static void bdb_start_top_level_commissioning_wrapper(uint8_t mode_mask);

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

esp_err_t zigbee_core_init(void)
{
    ESP_LOGI(TAG, "Initializing Zigbee core system...");
    
    // Initialize device info structure
    device_info.zigbee_joined = false;
    device_info.last_zigbee_report = 0;
    device_info.pan_id = 0;
    device_info.channel = 0;
    device_info.short_address = 0;
    
    // Initialize Zigbee stack configuration
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,
        .nwk_cfg = {
            .zed_cfg = {
                .ed_timeout = ED_AGING_TIMEOUT,
                .keep_alive = ED_KEEP_ALIVE,
            },
        },
    };
    
    // Initialize Zigbee stack
    esp_zb_init(&zb_nwk_cfg);
    
    // CRITICAL: Reduce TX power to avoid brownout on boards with weak power supply
    // This sacrifices range for stability on poorly designed boards
    esp_zb_set_tx_power(10);  // 10dBm instead of default 20dBm (void return)
    ESP_LOGI(TAG, "Reduced Zigbee TX power to 10dBm for board compatibility");
    ESP_LOGI(TAG, "Zigbee stack initialized successfully");
    
    ESP_LOGI(TAG, "Zigbee core system initialized successfully");
    return ESP_OK;
}

esp_err_t zigbee_core_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Zigbee core system...");
    
    // Stop main loop task if running
    zigbee_core_stop_main_loop_task();
    
    // Reset device info
    device_info.zigbee_joined = false;
    device_info.last_zigbee_report = 0;
    
    ESP_LOGI(TAG, "Zigbee core system deinitialized");
    return ESP_OK;
}

esp_err_t zigbee_core_start(void)
{
    ESP_LOGI(TAG, "Starting Zigbee stack...");
    
    // Create basic and identify configurations
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY,
    };
    
    esp_zb_identify_cluster_cfg_t identify_cfg = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    
    // Create endpoint list
    esp_zb_ep_list_t *esp_zb_sensor_ep = zigbee_core_create_sensor_endpoint(
        HA_ESP_SENSOR_ENDPOINT, &basic_cfg, &identify_cfg);
    if (!esp_zb_sensor_ep) {
        ESP_LOGE(TAG, "Failed to create sensor endpoint");
        return ESP_FAIL;
    }
    
    // Register the device
    esp_zb_device_register(esp_zb_sensor_ep);
    
    // Set primary network channel
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    
    // Set initial attribute values
    esp_err_t ret = zigbee_core_set_initial_attributes();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set some initial attributes: %s", esp_err_to_name(ret));
    }
    
    // Start Zigbee stack
    ret = esp_zb_start(false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Zigbee stack: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Zigbee stack started successfully");
    return ESP_OK;
}

esp_err_t zigbee_core_stop(void)
{
    ESP_LOGI(TAG, "Stopping Zigbee stack...");
    
    // Stop main loop task
    zigbee_core_stop_main_loop_task();
    
    // Reset join status
    device_info.zigbee_joined = false;
    
    ESP_LOGI(TAG, "Zigbee stack stopped");
    return ESP_OK;
}

esp_err_t zigbee_core_start_main_loop_task(void)
{
    if (zigbee_main_loop_task_handle != NULL) {
        ESP_LOGW(TAG, "Zigbee main loop task already running");
        return ESP_OK;
    }
    
    BaseType_t ret = xTaskCreate(
        zigbee_main_loop_task,
        "zigbee_main",
        ZIGBEE_TASK_STACK,
        NULL,
        ZIGBEE_TASK_PRIORITY,
        &zigbee_main_loop_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Zigbee main loop task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Zigbee main loop task started");
    return ESP_OK;
}

esp_err_t zigbee_core_stop_main_loop_task(void)
{
    if (zigbee_main_loop_task_handle != NULL) {
        vTaskDelete(zigbee_main_loop_task_handle);
        zigbee_main_loop_task_handle = NULL;
        ESP_LOGI(TAG, "Zigbee main loop task stopped");
    }
    
    return ESP_OK;
}

bool zigbee_core_get_device_info(zigbee_device_info_t *info)
{
    if (!info) {
        return false;
    }
    
    *info = device_info;
    return true;
}

bool zigbee_core_is_joined(void)
{
    return device_info.zigbee_joined;
}

esp_err_t zigbee_core_set_initial_attributes(void)
{
    ESP_LOGI(TAG, "Setting initial attribute values...");
    
    // Set device enabled attribute (basic cluster)
    uint8_t device_enabled = 1;
    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID,
        &device_enabled, false);
    
    if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "Setting device enabled attribute failed!");
    }
    
    // Set battery percentage attribute
    status = esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
        &(uint8_t){0}, false);
    if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "Setting battery percentage attribute failed!");
    }
    
    // Set battery voltage attribute
    status = esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
        &(uint8_t){0}, false);
    if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "Setting battery voltage attribute failed!");
    } else {
        ESP_LOGI(TAG, "Battery attributes initialized successfully");
    }
    
    ESP_LOGI(TAG, "Initial attributes set");
    return ESP_OK;
}

esp_zb_cluster_list_t *zigbee_core_create_sensor_clusters(
    esp_zb_basic_cluster_cfg_t *basic_cfg, 
    esp_zb_identify_cluster_cfg_t *identify_cfg)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    if (!cluster_list) {
        ESP_LOGE(TAG, "Failed to create cluster list");
        return NULL;
    }
    
    // Basic cluster (required)
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(basic_cfg);
    if (!basic_cluster) {
        ESP_LOGE(TAG, "Failed to create basic cluster");
        return NULL;
    }
    
    // Add manufacturer and model information
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, 
        ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, 
        ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER));
    
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, 
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    // Identify cluster (required)
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(identify_cfg);
    if (!identify_cluster) {
        ESP_LOGE(TAG, "Failed to create identify cluster");
        return NULL;
    }
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, 
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    // Power Configuration cluster for battery reporting
    esp_zb_power_config_cluster_cfg_t power_config_cfg = {
        .main_voltage = 0xffff,
    };
    esp_zb_attribute_list_t *power_config_cluster = esp_zb_power_config_cluster_create(&power_config_cfg);
    if (!power_config_cluster) {
        ESP_LOGE(TAG, "Failed to create power config cluster");
        return NULL;
    }
    
    // Battery voltage with REPORTING access
    uint8_t battery_voltage_init = 0;
    uint8_t battery_percentage_init = 0;
    
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(power_config_cluster,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
        ESP_ZB_ZCL_ATTR_TYPE_U8,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
        &battery_voltage_init));
    
    // Battery percentage
    ESP_ERROR_CHECK(esp_zb_power_config_cluster_add_attr(power_config_cluster,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
        &battery_percentage_init));
    
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_power_config_cluster(cluster_list, power_config_cluster, 
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    // On/Off cluster for remote LED control
    esp_zb_on_off_cluster_cfg_t on_off_cfg = {
        .on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    if (!on_off_cluster) {
        ESP_LOGE(TAG, "Failed to create on/off cluster");
        return NULL;
    }
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, 
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    // Temperature Measurement cluster for soil temperature
    // Min/Max: -40°C to +80°C in 0.01°C units (-4000 to 8000)
    esp_zb_temperature_meas_cluster_cfg_t temp_cfg = {
        .measured_value = ESP_ZB_ZCL_TEMP_MEASUREMENT_MEASURED_VALUE_DEFAULT,
        .min_value = -4000,  // -40°C
        .max_value = 8000,   // +80°C
    };
    esp_zb_attribute_list_t *temp_cluster = esp_zb_temperature_meas_cluster_create(&temp_cfg);
    if (!temp_cluster) {
        ESP_LOGW(TAG, "Failed to create temperature cluster");
    } else {
        ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, temp_cluster, 
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    }
    
    // Relative Humidity cluster (repurposed for soil moisture)
    // Min/Max: 0-100% in 0.01% units (0-10000)
    esp_zb_humidity_meas_cluster_cfg_t humidity_cfg = {
        .measured_value = ESP_ZB_ZCL_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_DEFAULT,
        .min_value = 0,      // 0%
        .max_value = 10000,  // 100%
    };
    esp_zb_attribute_list_t *humidity_cluster = esp_zb_humidity_meas_cluster_create(&humidity_cfg);
    if (!humidity_cluster) {
        ESP_LOGW(TAG, "Failed to create humidity cluster");
    } else {
        ESP_ERROR_CHECK(esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list, humidity_cluster, 
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    }
    
    ESP_LOGI(TAG, "All clusters created successfully (Basic, Identify, PowerConfig, OnOff, Temperature, Humidity)");
    return cluster_list;
}

esp_zb_ep_list_t *zigbee_core_create_sensor_endpoint(
    uint8_t endpoint_id,
    esp_zb_basic_cluster_cfg_t *basic_cfg, 
    esp_zb_identify_cluster_cfg_t *identify_cfg)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    if (!ep_list) {
        ESP_LOGE(TAG, "Failed to create endpoint list");
        return NULL;
    }
    
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = endpoint_id,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,  // Changed from ON_OFF_LIGHT to SIMPLE_SENSOR
        .app_device_version = 0
    };
    
    esp_zb_cluster_list_t *cluster_list = zigbee_core_create_sensor_clusters(basic_cfg, identify_cfg);
    if (!cluster_list) {
        ESP_LOGE(TAG, "Failed to create sensor clusters");
        return NULL;
    }
    
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    ESP_LOGI(TAG, "Sensor endpoint created successfully");
    return ep_list;
}

void zigbee_core_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
        
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", 
                     esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted - already joined");
                device_info.zigbee_joined = true;
                device_info.pan_id = esp_zb_get_pan_id();
                device_info.channel = esp_zb_get_current_channel();
                device_info.short_address = esp_zb_get_short_address();
                
                ESP_LOGI(TAG, "Zigbee reporting ready");
            }
        } else {
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;
        
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "✅✅✅ JOINED NETWORK SUCCESSFULLY! ✅✅✅");
            ESP_LOGI(TAG, "Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0]);
            
            device_info.zigbee_joined = true;
            device_info.pan_id = esp_zb_get_pan_id();
            device_info.channel = esp_zb_get_current_channel();
            device_info.short_address = esp_zb_get_short_address();
            
            ESP_LOGI(TAG, "PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx",
                     device_info.pan_id, device_info.channel, device_info.short_address);
            ESP_LOGI(TAG, "✅ Device should now appear in Zigbee2MQTT!");
            ESP_LOGI(TAG, "Zigbee reporting ready");
        } else {
            ESP_LOGW(TAG, "❌ Network steering FAILED: %s", esp_err_to_name(err_status));
            ESP_LOGI(TAG, "Retrying in 3 seconds... (Make sure Permit Join is enabled in Z2M!)");
            esp_zb_scheduler_alarm(bdb_start_top_level_commissioning_wrapper, ESP_ZB_BDB_MODE_NETWORK_STEERING, 3000);
        }
        break;
        
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

esp_err_t zigbee_core_register_action_handler(esp_err_t (*handler_func)(esp_zb_core_action_callback_id_t, const void *))
{
    if (!handler_func) {
        ESP_LOGE(TAG, "Handler function is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    action_handler_callback = handler_func;
    esp_zb_core_action_handler_register(handler_func);
    ESP_LOGI(TAG, "Action handler registered successfully");
    return ESP_OK;
}

esp_err_t zigbee_core_update_soil_moisture(float moisture_percent)
{
    // Zigbee Humidity is in 0.01% units (0-10000)
    // Convert from 0-100% to 0-10000
    uint16_t humidity_value = (uint16_t)(moisture_percent * 100.0f);
    
    // Clamp to valid range
    if (humidity_value > 10000) {
        humidity_value = 10000;
    }
    
    // Update Zigbee attribute (Humidity cluster ID 0x0405, Measured Value attribute 0x0000)
    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
        &humidity_value,
        false  // Let Z2M poll
    );
    
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Soil moisture updated: %.1f%% (ZB value: %d)", moisture_percent, humidity_value);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Failed to update soil moisture: %d", status);
        return ESP_FAIL;
    }
}

esp_err_t zigbee_core_update_soil_temperature(float temp_celsius)
{
    // Zigbee Temperature is in 0.01°C units
    // Convert from Celsius to int16_t (centidegrees)
    int16_t temp_value = (int16_t)(temp_celsius * 100.0f);
    
    // Update Zigbee attribute (Temperature cluster ID 0x0402, Measured Value attribute 0x0000)
    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        &temp_value,
        false  // Let Z2M poll
    );
    
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Soil temperature updated: %.1f°C (ZB value: %d)", temp_celsius, temp_value);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Failed to update soil temperature: %d", status);
        return ESP_FAIL;
    }
}

// ============================================================================
// PRIVATE FUNCTIONS
// ============================================================================

static void zigbee_main_loop_task(void *param)
{
    ESP_LOGI(TAG, "Zigbee main loop task started");
    while (1) {
        // Process Zigbee stack events
        esp_zb_stack_main_loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void bdb_start_top_level_commissioning_wrapper(uint8_t mode_mask)
{
    esp_zb_bdb_start_top_level_commissioning(mode_mask);
}

