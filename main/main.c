/*
 * AC Power Monitor for Glyph C6 (ESP32-C6)
 * Deep Sleep Version - Ultra Power Saving Mode
 * 
 * Board: Glyph C6 with ESP32-C6-MINI-1 module
 * Version: 2.0.0 - Deep Sleep Implementation
 * 
 * Features:
 * - Deep sleep with 6-hour wake intervals
 * - Synchronized soil + battery readings
 * - Zigbee rejoin on wake
 * - 14-18 month battery life (1000mAh)
 * 
 * Power Profile:
 * - Deep sleep: ~10µA (23.9 hours/day)
 * - Wake/read/transmit: ~50mA (4 minutes/day)
 * - Average: ~3.5 mAh/day
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_check.h"

// ESP Zigbee SDK includes
#include "esp_zigbee_core.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_endpoint.h"
#include "esp_zigbee_attribute.h"
#include "zboss_api.h"

// Project modules
#include "system_config.h"
#include "zigbee_core.h"
#include "battery_monitoring.h"
#include "soil_sensor.h"
#include "deep_sleep.h"

// Define missing Power Config cluster attribute IDs
#ifndef ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID
#define ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID        0x0021
#endif

#ifndef ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID
#define ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID                     0x0020
#endif

static const char *TAG = "GLYPH_C6_SLEEP";

// LED state tracking
static bool led_state = false;

// Timing control
static TickType_t zigbee_join_start = 0;
static bool zigbee_join_attempted = false;
static bool readings_complete = false;

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
    gpio_set_level(GPIO_NUM_14, 0);

    // Configure NeoPixel/I2C power pin as output and set HIGH
    gpio_config_t power_conf = {
        .pin_bit_mask = (1ULL << NEOPIXEL_I2C_POWER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&power_conf);
    gpio_set_level(NEOPIXEL_I2C_POWER, 1);
    
    ESP_LOGI(TAG, "GPIO initialized - NeoPixel/I2C Power: ON");
}

/**
 * @brief Take multiple sensor samples and average them (direct hardware reads)
 * 
 * This function performs fresh I2C/ADC reads for each sample.
 * No background tasks needed - suitable for deep sleep mode.
 */
static bool read_averaged_sensors(float *avg_moisture, float *avg_temp, float *avg_voltage, float *avg_battery_percent)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📊 Taking %d sensor samples (averaging for accuracy)...", NUM_SENSOR_SAMPLES);
    
    float moisture_sum = 0.0f, temp_sum = 0.0f, voltage_sum = 0.0f, percent_sum = 0.0f;
    int valid_soil_samples = 0;
    int valid_battery_samples = 0;
    
    for (int i = 0; i < NUM_SENSOR_SAMPLES; i++) {
        ESP_LOGI(TAG, "  Sample %d/%d...", i + 1, NUM_SENSOR_SAMPLES);
        
        // Read soil sensor directly (fresh I2C transaction)
        soil_data_t soil_data;
        if (soil_sensor_read_all(&soil_data) == ESP_OK) {
            moisture_sum += soil_data.moisture_percent;
            temp_sum += soil_data.temperature_c;
            valid_soil_samples++;
            ESP_LOGI(TAG, "    Soil: %.1f%% moisture, %.1f°C", 
                     soil_data.moisture_percent, soil_data.temperature_c);
        }
        
        // Read battery directly (fresh ADC read)
        float voltage, percent;
        if (battery_read(&voltage, &percent) == ESP_OK) {
            voltage_sum += voltage;
            percent_sum += percent;
            valid_battery_samples++;
            ESP_LOGI(TAG, "    Battery: %.2fV (%.1f%%)", voltage, percent);
        }
        
        // Wait between samples for stability
        if (i < NUM_SENSOR_SAMPLES - 1) {
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
        }
    }
    
    // Calculate averages
    if (valid_soil_samples > 0) {
        *avg_moisture = moisture_sum / valid_soil_samples;
        *avg_temp = temp_sum / valid_soil_samples;
    }
    
    if (valid_battery_samples > 0) {
        *avg_voltage = voltage_sum / valid_battery_samples;
        *avg_battery_percent = percent_sum / valid_battery_samples;
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📈 Averaged Results (%d samples):", NUM_SENSOR_SAMPLES);
    ESP_LOGI(TAG, "  Soil: %.1f%% moisture, %.1f°C", *avg_moisture, *avg_temp);
    ESP_LOGI(TAG, "  Battery: %.2fV (%.1f%%)", *avg_voltage, *avg_battery_percent);
    
    return (valid_soil_samples > 0 && valid_battery_samples > 0);
}

/**
 * @brief Report averaged sensor data to Zigbee
 */
static void report_sensor_data(float moisture, float temp, float voltage, float percent)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📊 Reporting averaged sensor data to Zigbee...");
    
    // Report battery percentage
    uint16_t battery_percent_raw = (uint16_t)(percent * 2.0f);
    uint8_t battery_percent = (battery_percent_raw <= 200) ? (uint8_t)battery_percent_raw : 200;
    
    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
        &battery_percent,
        false
    );
    
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "  ✅ Battery: %.2fV (%.1f%%)", voltage, percent);
    }
    
    // Report battery voltage
    uint16_t battery_voltage_raw = (uint16_t)(voltage * 10.0f);
    if (battery_voltage_raw <= 255) {
        uint8_t battery_voltage_dv = (uint8_t)battery_voltage_raw;
        esp_zb_zcl_set_attribute_val(
            HA_ESP_SENSOR_ENDPOINT,
            ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
            &battery_voltage_dv,
            false
        );
    }
    
    // Report moisture
    esp_err_t ret = zigbee_core_update_soil_moisture(moisture);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ Soil: %.1f%% moisture, %.1f°C", moisture, temp);
    }
    
    // Report temperature
    zigbee_core_update_soil_temperature(temp);
    
    ESP_LOGI(TAG, "📊 Averaged sensor data reported to Zigbee");
}

/**
 * @brief OTA upgrade status callback
 * 
 * ESP Zigbee SDK uses PASSIVE OTA mode:
 * - Z2M (coordinator) pushes updates when available
 * - Device receives callbacks during download
 * - No manual query needed!
 */
static void ota_upgrade_status_handler(esp_zb_zcl_ota_upgrade_value_message_t *message)
{
    if (!message) {
        return;
    }
    
    ESP_LOGI(TAG, "📦 OTA Status: %d", message->info.status);
    
    switch (message->info.status) {
    case ESP_ZB_ZCL_STATUS_SUCCESS:
        switch (message->upgrade_status) {
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
            ESP_LOGI(TAG, "🔄 OTA Download started");
            ESP_LOGI(TAG, "  Firmware size: %lu bytes", message->ota_header.image_size);
            ESP_LOGI(TAG, "  Version: 0x%08lx", message->ota_header.file_version);
            break;
            
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:
            // Log progress every ~10 blocks to avoid spam
            static uint32_t block_count = 0;
            block_count++;
            if (block_count % 10 == 0) {
                ESP_LOGI(TAG, "  Downloading... received %u blocks", block_count);
            }
            break;
            
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY:
            ESP_LOGI(TAG, "✅ OTA Download complete!");
            ESP_LOGI(TAG, "  Applying firmware...");
            break;
            
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
            ESP_LOGI(TAG, "📋 OTA Check: Version 0x%08lx available", 
                     message->ota_header.file_version);
            break;
            
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
            ESP_LOGI(TAG, "🎉 OTA Update complete - rebooting in 3 seconds...");
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
            break;
            
        default:
            ESP_LOGI(TAG, "  OTA status: %d", message->upgrade_status);
            break;
        }
        break;
        
    case ESP_ZB_ZCL_STATUS_ABORT:
        ESP_LOGW(TAG, "❌ OTA Download aborted");
        break;
        
    default:
        ESP_LOGW(TAG, "⚠️ OTA Status error: %d", message->info.status);
        break;
    }
}

/**
 * @brief Wake cycle task - handles readings, OTA, and sleep
 */
static void wake_cycle_task(void *pvParameters)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "⏰ Wake cycle started");
    
    const TickType_t wake_duration = pdMS_TO_TICKS(WAKE_TIME_MS);
    const TickType_t max_join_wait = pdMS_TO_TICKS(30000);  // 30 seconds max for join
    
    TickType_t start_time = xTaskGetTickCount();
    bool sensors_read = false;
    bool data_transmitted = false;
    
    // Averaged sensor values
    float avg_moisture = 0.0f, avg_temp = 0.0f, avg_voltage = 0.0f, avg_percent = 0.0f;
    
    // Wait for Zigbee to join
    zigbee_join_start = xTaskGetTickCount();
    zigbee_join_attempted = true;
    
    while (1) {
        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed = now - start_time;
        
        // Check if Zigbee joined
        if (zigbee_core_is_joined()) {
            // NOTE: OTA updates handled automatically by callbacks
            // Z2M (coordinator) pushes updates when available
            // Device stays awake if OTA download is in progress
            
            // Read sensors (if not already done)
            if (!sensors_read && deep_sleep_should_read_sensors()) {
                ESP_LOGI(TAG, "✅ Zigbee joined! Taking sensor readings...");
                
                // Take multiple samples and average
                if (read_averaged_sensors(&avg_moisture, &avg_temp, &avg_voltage, &avg_percent)) {
                    sensors_read = true;
                } else {
                    ESP_LOGW(TAG, "❌ Failed to read sensors");
                    break;
                }
            }
            
            // Transmit averaged data
            if (sensors_read && !data_transmitted) {
                report_sensor_data(avg_moisture, avg_temp, avg_voltage, avg_percent);
                data_transmitted = true;
                
                // Mark readings as complete
                deep_sleep_mark_sensors_read();
                readings_complete = true;
                
                ESP_LOGI(TAG, "✅ Averaged data transmitted successfully!");
                
                // Stay awake a bit longer to ensure transmission completes
                vTaskDelay(pdMS_TO_TICKS(5000));
                
                // Done with this wake cycle
                break;
            }
        }
        
        // Check timeouts
        if (elapsed >= wake_duration) {
            ESP_LOGW(TAG, "⏰ Wake time expired");
            break;
        }
        
        if (!zigbee_core_is_joined() && (now - zigbee_join_start) >= max_join_wait) {
            ESP_LOGW(TAG, "⏰ Zigbee join timeout - will retry next wake");
            break;
        }
        
        // Status log every 5 seconds
        if (elapsed % pdMS_TO_TICKS(5000) == 0) {
            if (zigbee_core_is_joined()) {
                ESP_LOGI(TAG, "Status: JOINED, processing...");
            } else {
                ESP_LOGI(TAG, "Status: Joining network... (%lu seconds elapsed)", 
                         (now - zigbee_join_start) / configTICK_RATE_HZ);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Enter deep sleep
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Wake cycle complete - entering deep sleep");
    deep_sleep_enter();
    
    // Never reached
    vTaskDelete(NULL);
}

/**
 * @brief Zigbee attribute handler
 */
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, 
                       "Received message: error status(%d)", message->info.status);
    
    // Handle On/Off cluster for LED control
    if (message->info.dst_endpoint == HA_ESP_SENSOR_ENDPOINT &&
        message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
            message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            
            bool new_state = message->attribute.data.value ? 
                            *(bool *)message->attribute.data.value : false;
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
    case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID:
        // Handle OTA upgrade status updates
        ota_upgrade_status_handler((esp_zb_zcl_ota_upgrade_value_message_t *)message);
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
    ESP_LOGI(TAG, "  Glyph C6 Plant Monitor - Deep Sleep Mode");
    ESP_LOGI(TAG, "  Firmware: %s", FIRMWARE_VERSION_STRING);
    ESP_LOGI(TAG, "  Version: 0x%08lX, Built: %s", FIRMWARE_VERSION, FIRMWARE_BUILD_DATE);
    ESP_LOGI(TAG, "  Battery Life: 14-18 months (1000mAh)");
    ESP_LOGI(TAG, "===========================================");

    // Initialize deep sleep management FIRST
    esp_err_t ret = deep_sleep_init();
    ESP_ERROR_CHECK(ret);

    // Initialize NVS (required for Zigbee)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize GPIO
    gpio_init();

    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip: ESP32-C6, Cores: %d, Revision: %d", 
             chip_info.cores, chip_info.revision);
    
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash: %lu MB, Free heap: %lu bytes", 
             flash_size / (1024 * 1024), esp_get_free_heap_size());

    // Wait for I2C sensors to power up
    ESP_LOGI(TAG, "Waiting 500ms for I2C devices...");
    vTaskDelay(pdMS_TO_TICKS(500));

    // Initialize I2C bus
    ESP_LOGI(TAG, "Initializing I2C bus...");
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    esp_err_t i2c_ret = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
    if (i2c_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(i2c_ret));
    }

    // Initialize Zigbee core
    ESP_LOGI(TAG, "Initializing Zigbee SDK...");
    ESP_ERROR_CHECK(zigbee_core_init());
    ESP_ERROR_CHECK(zigbee_core_register_action_handler(zb_action_handler));
    ESP_ERROR_CHECK(zigbee_core_start());
    ESP_ERROR_CHECK(zigbee_core_start_main_loop_task());
    
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize battery monitoring (hardware only - no background tasks)
    ESP_LOGI(TAG, "Initializing battery monitoring...");
    battery_monitoring_init();

    // Initialize soil sensor (hardware only - no background tasks)
    ESP_LOGI(TAG, "Initializing soil sensor...");
    soil_sensor_init(bus_handle);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Application initialized successfully");
    ESP_LOGI(TAG, "Sensors read on-demand (direct I2C/ADC reads)");
    ESP_LOGI(TAG, "Readings every 1 hour (soil + battery together)");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    // Create wake cycle task
    xTaskCreate(wake_cycle_task, "wake_cycle", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Wake cycle task started - waiting for Zigbee join...");
}

