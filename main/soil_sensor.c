/*
 * Adafruit STEMMA Soil Sensor (4026) Driver Implementation
 */

#include "soil_sensor.h"
#include "system_config.h"
#include "zigbee_core.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "SOIL_SENSOR";

// Seesaw protocol registers
#define SEESAW_STATUS_BASE          0x00
#define SEESAW_STATUS_SWRST         0x7F
#define SEESAW_STATUS_VERSION       0x02
#define SEESAW_STATUS_TEMP          0x04

#define SEESAW_TOUCH_BASE           0x0F
#define SEESAW_TOUCH_CHANNEL_OFFSET 0x10

// Cached data and mutex
static soil_data_t cached_data = {0};
static SemaphoreHandle_t data_mutex = NULL;
static TaskHandle_t soil_task_handle = NULL;
static bool sensor_initialized = false;

/**
 * @brief Write command to Seesaw sensor
 */
static esp_err_t seesaw_write_cmd(uint8_t base, uint8_t func)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SOIL_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, base, true);
    i2c_master_write_byte(cmd, func, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

/**
 * @brief Write command with data to Seesaw sensor
 */
static esp_err_t seesaw_write_cmd_data(uint8_t base, uint8_t func, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SOIL_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, base, true);
    i2c_master_write_byte(cmd, func, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

/**
 * @brief Read data from Seesaw sensor
 */
static esp_err_t seesaw_read_data(uint8_t *buffer, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SOIL_SENSOR_ADDR << 1) | I2C_MASTER_READ, true);
    
    if (len > 1) {
        i2c_master_read(cmd, buffer, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, buffer + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

// Initialize sensor
esp_err_t soil_sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing Adafruit Soil Sensor...");
    
    // Create mutex for thread safety
    if (data_mutex == NULL) {
        data_mutex = xSemaphoreCreateMutex();
        if (data_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_FAIL;
        }
    }
    
    // Check if sensor is present
    if (!soil_sensor_is_present()) {
        ESP_LOGE(TAG, "Soil sensor not found at address 0x%02X", SOIL_SENSOR_ADDR);
        return ESP_FAIL;
    }
    
    // Perform soft reset
    ESP_LOGI(TAG, "Performing soft reset...");
    esp_err_t ret = seesaw_write_cmd_data(SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, 0xFF);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Soft reset failed (may be expected): %s", esp_err_to_name(ret));
    }
    
    // Wait longer for sensor to fully boot and stabilize
    ESP_LOGI(TAG, "Waiting for sensor to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Initialize cached data
    cached_data.valid = false;
    cached_data.timestamp = 0;
    
    sensor_initialized = true;
    ESP_LOGI(TAG, "Soil sensor initialized successfully");
    
    return ESP_OK;
}

// Read moisture
esp_err_t soil_sensor_read_moisture(uint16_t *raw_value, float *percent)
{
    if (!sensor_initialized) {
        ESP_LOGE(TAG, "Sensor not initialized");
        return ESP_FAIL;
    }
    
    // Request capacitance reading
    esp_err_t ret = seesaw_write_cmd(SEESAW_TOUCH_BASE, SEESAW_TOUCH_CHANNEL_OFFSET);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to request moisture reading: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for conversion (increase delay for reliability)
    vTaskDelay(pdMS_TO_TICKS(5));
    
    // Read 2 bytes
    uint8_t data[2];
    ret = seesaw_read_data(data, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read moisture data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Combine bytes (big-endian)
    uint16_t raw = (data[0] << 8) | data[1];
    
    // Convert to percentage
    float pct = ((float)(raw - SOIL_VALUE_DRY) / (float)(SOIL_VALUE_WET - SOIL_VALUE_DRY)) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    
    if (raw_value) *raw_value = raw;
    if (percent) *percent = pct;
    
    return ESP_OK;
}

// Read temperature
esp_err_t soil_sensor_read_temperature(float *temp_c, float *temp_f)
{
    if (!sensor_initialized) {
        ESP_LOGE(TAG, "Sensor not initialized");
        return ESP_FAIL;
    }
    
    // Request temperature reading
    esp_err_t ret = seesaw_write_cmd(SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to request temperature reading: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for conversion
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Read 4 bytes
    uint8_t data[4];
    ret = seesaw_read_data(data, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Combine bytes (big-endian, signed)
    int32_t temp_raw = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    float celsius = (float)temp_raw / 65536.0f;
    float fahrenheit = (celsius * 9.0f / 5.0f) + 32.0f;
    
    if (temp_c) *temp_c = celsius;
    if (temp_f) *temp_f = fahrenheit;
    
    return ESP_OK;
}

// Read all data
esp_err_t soil_sensor_read_all(soil_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    soil_data_t temp_data = {0};
    temp_data.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Read moisture
    esp_err_t ret = soil_sensor_read_moisture(&temp_data.moisture_raw, &temp_data.moisture_percent);
    if (ret != ESP_OK) {
        temp_data.valid = false;
        return ret;
    }
    
    // Read temperature
    ret = soil_sensor_read_temperature(&temp_data.temperature_c, &temp_data.temperature_f);
    if (ret != ESP_OK) {
        // Temperature read failed, but moisture is valid
        ESP_LOGW(TAG, "Temperature read failed, continuing with moisture data");
        temp_data.temperature_c = 0.0f;
        temp_data.temperature_f = 32.0f;
    }
    
    temp_data.valid = true;
    
    // Update cached data (thread-safe)
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        cached_data = temp_data;
        xSemaphoreGive(data_mutex);
    }
    
    *data = temp_data;
    return ESP_OK;
}

// Get cached data
esp_err_t soil_sensor_get_cached_data(soil_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *data = cached_data;
        xSemaphoreGive(data_mutex);
        
        if (!cached_data.valid) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

// Get moisture status
soil_status_t soil_sensor_get_status(float percent)
{
    if (percent < SOIL_MOISTURE_CRITICAL) {
        return SOIL_STATUS_CRITICAL;
    } else if (percent < SOIL_MOISTURE_LOW) {
        return SOIL_STATUS_LOW;
    } else if (percent < SOIL_MOISTURE_GOOD) {
        return SOIL_STATUS_GOOD;
    } else if (percent < SOIL_MOISTURE_HIGH) {
        return SOIL_STATUS_HIGH;
    } else {
        return SOIL_STATUS_SATURATED;
    }
}

// Get status string
const char* soil_sensor_status_string(soil_status_t status)
{
    switch (status) {
        case SOIL_STATUS_CRITICAL:  return "💀 CRITICAL - Water NOW!";
        case SOIL_STATUS_LOW:       return "💧 Low - Water soon";
        case SOIL_STATUS_GOOD:      return "✅ Good - Happy plant";
        case SOIL_STATUS_HIGH:      return "💦 High - Don't water";
        case SOIL_STATUS_SATURATED: return "🌊 Saturated - Too wet";
        case SOIL_STATUS_ERROR:     return "❌ Error";
        default:                    return "❓ Unknown";
    }
}

// Soil monitoring task
static void soil_monitoring_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Soil monitoring task started");
    
    soil_data_t data;
    int consecutive_failures = 0;
    
    while (1) {
        // Read all sensor data
        esp_err_t ret = soil_sensor_read_all(&data);
        
        if (ret == ESP_OK && data.valid) {
            consecutive_failures = 0;  // Reset failure counter
            soil_status_t status = soil_sensor_get_status(data.moisture_percent);
            
            ESP_LOGI(TAG, "📊 Soil Reading:");
            ESP_LOGI(TAG, "   Moisture: %d raw (%.1f%%) - %s",
                     data.moisture_raw, data.moisture_percent,
                     soil_sensor_status_string(status));
            ESP_LOGI(TAG, "   Temperature: %.1f°C (%.1f°F)",
                     data.temperature_c, data.temperature_f);
            
            // Report to Zigbee/Home Assistant if network is joined
            if (zigbee_core_is_joined()) {
                zigbee_core_update_soil_moisture(data.moisture_percent);
                zigbee_core_update_soil_temperature(data.temperature_c);
                ESP_LOGI(TAG, "   → Reported to Zigbee/Z2M");
            }
        } else {
            consecutive_failures++;
            ESP_LOGW(TAG, "Failed to read soil sensor (failure #%d)", consecutive_failures);
            
            // Try to recover after 3 consecutive failures
            if (consecutive_failures >= 3) {
                ESP_LOGW(TAG, "Multiple failures detected, attempting sensor reset...");
                
                // Try soft reset
                esp_err_t reset_ret = seesaw_write_cmd_data(SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, 0xFF);
                if (reset_ret == ESP_OK) {
                    ESP_LOGI(TAG, "Sensor reset successful, waiting 1s...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    consecutive_failures = 0;  // Reset counter after recovery attempt
                } else {
                    ESP_LOGE(TAG, "Sensor reset failed: %s", esp_err_to_name(reset_ret));
                }
            }
        }
        
        // Wait for next reading
        vTaskDelay(pdMS_TO_TICKS(SOIL_READ_INTERVAL));
    }
}

// Start monitoring task
esp_err_t soil_sensor_start_task(void)
{
    if (soil_task_handle != NULL) {
        ESP_LOGW(TAG, "Soil monitoring task already running");
        return ESP_OK;
    }
    
    BaseType_t ret = xTaskCreate(
        soil_monitoring_task,
        "soil_mon",
        SOIL_TASK_STACK,
        NULL,
        SOIL_TASK_PRIORITY,
        &soil_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create soil monitoring task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Soil monitoring task created (reads every %d seconds)", SOIL_READ_INTERVAL / 1000);
    return ESP_OK;
}

// Stop monitoring task
esp_err_t soil_sensor_stop_task(void)
{
    if (soil_task_handle != NULL) {
        vTaskDelete(soil_task_handle);
        soil_task_handle = NULL;
        ESP_LOGI(TAG, "Soil monitoring task stopped");
    }
    return ESP_OK;
}

// Check if sensor is present
bool soil_sensor_is_present(void)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SOIL_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK);
}

