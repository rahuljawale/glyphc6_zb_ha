/*
 * AC Power Monitor for Glyph C6 (ESP32-C6)
 * Based on Adafruit ESP32-C6 Feather pinout
 * 
 * Board: Glyph C6 with ESP32-C6-MINI-1 module
 * Version: 1.4.0 - Working Zigbee Build
 * 
 * Features:
 * - Zigbee connectivity for Home Assistant/Zigbee2MQTT
 * - Status monitoring via serial output
 * - NeoPixel/I2C power management (GPIO20)
 * 
 * CRITICAL FIXES APPLIED:
 * - NVRAM corruption fix (required esp_zb_nvram_erase_at_start on first flash)
 * - Reduced TX power (10dBm) for board compatibility
 * - Brownout detector disabled (CONFIG_ESP_BROWNOUT_DET=n)
 * - Increased Zigbee task stack (8192 bytes)
 * - Device type changed from ON_OFF_LIGHT to SIMPLE_SENSOR
 * - Main loop task started before any BDB commissioning calls
 * 
 * TROUBLESHOOTING:
 * - If device crashes at "Start network steering": Run with esp_zb_nvram_erase_at_start(true) once
 * - If brownout errors: Check power supply (needs 2A+ USB adapter)
 * - If still issues: May be board hardware problem (try Adafruit Feather)
 * 
 * Pin Definitions:
 * - GPIO15: Onboard Red LED
 * - GPIO20: NeoPixel/I2C Power Control (must be HIGH)
 * - GPIO19: I2C SDA (STEMMA QT)
 * - GPIO18: I2C SCL (STEMMA QT)
 * - GPIO17: UART RX
 * - GPIO16: UART TX
 * - GPIO9:  NeoPixel/Boot button (shared)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_check.h"

// ESP Zigbee SDK includes
#include "esp_zigbee_core.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_endpoint.h"
#include "esp_zigbee_attribute.h"
#include "zboss_api.h"  // For ZB_RADIO_MODE_NATIVE and other constants

// Project modules
#include "system_config.h"
#include "zigbee_core.h"
#include "battery_monitoring.h"
#include "soil_sensor.h"

// Define missing Power Config cluster attribute IDs (not in ESP Zigbee SDK headers)
#ifndef ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID
#define ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID        0x0021
#endif

#ifndef ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID
#define ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID                     0x0020
#endif

static const char *TAG = "GLYPH_C6";

// LED state tracking
static bool led_state = false;

/**
 * @brief Set LED state
 */
static void set_led(bool state)
{
    led_state = state;
    gpio_set_level(GPIO_NUM_14, state ? 1 : 0);
    ESP_LOGI(TAG, "LED: %s", state ? "ON 💡" : "OFF");
}

/**
 * @brief Initialize GPIO pins (LED + NeoPixel/I2C power)
 */
static void gpio_init(void)
{
    ESP_LOGI(TAG, "Initializing GPIO pins...");

    // Configure LED (GPIO14) as output
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_14),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(GPIO_NUM_14, 0);  // LED off initially

    // Configure NeoPixel/I2C power pin as output and set HIGH
    gpio_config_t power_conf = {
        .pin_bit_mask = (1ULL << NEOPIXEL_I2C_POWER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&power_conf);
    gpio_set_level(NEOPIXEL_I2C_POWER, 1);  // Enable NeoPixel and I2C power
    
    ESP_LOGI(TAG, "GPIO initialization complete");
    ESP_LOGI(TAG, "LED: GPIO14 (off) - controlled via Z2M");
    ESP_LOGI(TAG, "NeoPixel/I2C Power: GPIO20 (enabled)");
}

// ============================================================================
// ZIGBEE ATTRIBUTE REPORTING
// ============================================================================

/**
 * @brief Report battery data to Zigbee (called via scheduler for safety)
 */
static void scheduled_battery_report(uint8_t param)
{
    (void)param;  // Unused
    
    float voltage = 0.0f;
    float percent = 0.0f;
    
    // Get battery data from cache (thread-safe)
    if (battery_get_cached_data(&voltage, &percent)) {
        // Report battery percentage (Zigbee uses 0-200 scale, 0.5% units)
        uint16_t battery_percent_raw = (uint16_t)(percent * 2.0f);
        uint8_t battery_percent = (battery_percent_raw <= 200) ? (uint8_t)battery_percent_raw : 200;
        
        // Update attribute value (Z2M will poll via binding)
        esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
            HA_ESP_SENSOR_ENDPOINT,
            ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
            &battery_percent,
            false  // false = update value, let binding handle reporting
        );
        
        if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
            ESP_LOGW(TAG, "Failed to set battery percentage: %d", status);
        }
        
        // Report battery voltage (Convert to decisvolts, 0.1V units)
        uint16_t battery_voltage_raw = (uint16_t)(voltage * 10.0f);
        if (battery_voltage_raw <= 255) {
            uint8_t battery_voltage_dv = (uint8_t)battery_voltage_raw;
            status = esp_zb_zcl_set_attribute_val(
                HA_ESP_SENSOR_ENDPOINT,
                ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
                &battery_voltage_dv,
                false  // false = update value, let binding handle reporting
            );
            
            if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "Failed to set battery voltage: %d", status);
            }
        }
        
        ESP_LOGI(TAG, "Battery attributes updated: %.2fV (%.1f%%) - percent=%d, voltage_dv=%d (Z2M will poll)", 
                 voltage, percent, battery_percent, (uint8_t)(voltage * 10.0f));
    }
}

// ============================================================================
// STATUS MONITORING TASK
// ============================================================================

/**
 * @brief Status monitoring task (logs Zigbee, LED, and Battery status)
 */
static void status_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting status monitoring task with battery reporting");
    TickType_t last_battery_report = 0;
    const TickType_t battery_report_interval = pdMS_TO_TICKS(60000);  // 1 minute
    
    while (1) {
        TickType_t now = xTaskGetTickCount();
        float voltage = 0.0f, percent = 0.0f;
        bool battery_valid = battery_get_cached_data(&voltage, &percent);
        bool usb_present = battery_is_usb_present();
        const char *power_source = usb_present ? "USB⚡" : "BAT🔋";
        
        if (zigbee_core_is_joined()) {
            ESP_LOGI(TAG, "Status: Zigbee JOINED ✅ | LED: %s | Power: %s %.2fV (%.1f%%)", 
                     led_state ? "ON 💡" : "OFF", power_source, voltage, percent);
            
            // Schedule battery report via Zigbee scheduler (safe from task context)
            if ((now - last_battery_report) >= battery_report_interval && battery_valid) {
                esp_zb_scheduler_alarm(scheduled_battery_report, 0, 10);  // 10ms delay
                last_battery_report = now;
            }
        } else {
            ESP_LOGI(TAG, "Status: Zigbee SEARCHING... 🔍 | LED: %s | Power: %s %.2fV (%.1f%%)", 
                     led_state ? "ON 💡" : "OFF", power_source, voltage, percent);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));  // Log every 5 seconds
    }
}

// ============================================================================
// ZIGBEE HANDLERS
// ============================================================================

/**
 * @brief Zigbee attribute handler (with LED control from Z2M/HA)
 */
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, 
                       "Received message: error status(%d)", message->info.status);
    
    ESP_LOGI(TAG, "Received attribute change (endpoint:%d, cluster:0x%04x, attr:0x%04x)", 
             message->info.dst_endpoint, message->info.cluster, message->attribute.id);
    
    // Handle On/Off cluster for LED control from Z2M/HA
    if (message->info.dst_endpoint == HA_ESP_SENSOR_ENDPOINT &&
        message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
            message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            
            bool new_state = message->attribute.data.value ? 
                            *(bool *)message->attribute.data.value : false;
            
            ESP_LOGI(TAG, "Remote control from Z2M/HA: LED %s", new_state ? "ON" : "OFF");
            set_led(new_state);
        }
    }
    
    return ret;
}

/**
 * @brief Zigbee action handler
 */
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

/**
 * @brief Zigbee signal handler
 */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    zigbee_core_app_signal_handler(signal_struct);
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  Glyph C6 Monitor with Zigbee");
    ESP_LOGI(TAG, "  Board: ESP32-C6-MINI-1");
    ESP_LOGI(TAG, "  Version: 1.4.0 - Production Ready");
    ESP_LOGI(TAG, "===========================================");

    // Initialize NVS (required for Zigbee)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize GPIO
    gpio_init();

    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip: ESP32-C6");
    ESP_LOGI(TAG, "CPU Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "Silicon Revision: %d", chip_info.revision);
    
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash: %lu MB %s", 
             flash_size / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    // Wait for I2C sensors to power up
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Waiting 500ms for I2C devices to power up...");
    vTaskDelay(pdMS_TO_TICKS(500));  // Give sensors time to boot after GPIO20 enabled

    // Initialize Zigbee core system
    ESP_LOGI(TAG, "Initializing Zigbee SDK...");
    
    esp_err_t zigbee_ret = zigbee_core_init();
    if (zigbee_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Zigbee core: %s", esp_err_to_name(zigbee_ret));
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // Register action handler
    esp_err_t handler_ret = zigbee_core_register_action_handler(zb_action_handler);
    if (handler_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register action handler: %s", esp_err_to_name(handler_ret));
    }
    
    // Start Zigbee stack
    zigbee_ret = zigbee_core_start();
    if (zigbee_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Zigbee core: %s", esp_err_to_name(zigbee_ret));
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    ESP_LOGI(TAG, "Zigbee SDK initialized - waiting for network join...");

    // CRITICAL: Start Zigbee main loop task IMMEDIATELY before anything else
    esp_err_t zigbee_task_ret = zigbee_core_start_main_loop_task();
    if (zigbee_task_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Zigbee main loop task: %s", esp_err_to_name(zigbee_task_ret));
    }
    
    // Give Zigbee main loop time to start processing
    ESP_LOGI(TAG, "Waiting for Zigbee main loop to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize battery monitoring
    ESP_LOGI(TAG, "Initializing battery monitoring...");
    esp_err_t battery_ret = battery_monitoring_init();
    if (battery_ret == ESP_OK) {
        ESP_LOGI(TAG, "Battery monitoring initialized successfully");
        
        // Start battery monitoring task
        battery_ret = battery_monitoring_start_task();
        if (battery_ret == ESP_OK) {
            ESP_LOGI(TAG, "Battery monitoring task started");
        } else {
            ESP_LOGW(TAG, "Failed to start battery monitoring task: %s", esp_err_to_name(battery_ret));
        }
    } else {
        ESP_LOGW(TAG, "Failed to initialize battery monitoring: %s", esp_err_to_name(battery_ret));
    }

    // Initialize soil sensor
    ESP_LOGI(TAG, "Initializing soil moisture sensor...");
    esp_err_t soil_ret = soil_sensor_init();
    if (soil_ret == ESP_OK) {
        ESP_LOGI(TAG, "Soil sensor initialized successfully");
        
        // Start soil monitoring task
        soil_ret = soil_sensor_start_task();
        if (soil_ret == ESP_OK) {
            ESP_LOGI(TAG, "Soil monitoring task started (reads every 60 seconds)");
        } else {
            ESP_LOGW(TAG, "Failed to start soil monitoring task: %s", esp_err_to_name(soil_ret));
        }
    } else {
        ESP_LOGW(TAG, "Soil sensor not found or failed to initialize");
        ESP_LOGW(TAG, "Continuing without soil monitoring...");
    }

    // Create status monitoring task
    xTaskCreate(status_task, "status_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Status monitoring task started");

    ESP_LOGI(TAG, "Application started successfully");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Zigbee device ready for commissioning");
    ESP_LOGI(TAG, "Use Zigbee2MQTT or Home Assistant to pair and control LED");
    ESP_LOGI(TAG, "Battery & Soil reporting enabled every 60 seconds");
}
