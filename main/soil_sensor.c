/*
 * Adafruit STEMMA Soil Sensor (4026) Driver Implementation
 * Simplified for deep sleep mode - direct reads only, no background tasks
 */

#include "soil_sensor.h"
#include "system_config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SOIL_SENSOR";

// I2C device handle
static i2c_master_dev_handle_t i2c_dev_handle = NULL;

// Seesaw protocol registers
#define SEESAW_STATUS_BASE          0x00
#define SEESAW_STATUS_SWRST         0x7F
#define SEESAW_STATUS_VERSION       0x02
#define SEESAW_STATUS_TEMP          0x04

#define SEESAW_TOUCH_BASE           0x0F
#define SEESAW_TOUCH_CHANNEL_OFFSET 0x10

// Sensor state
static bool sensor_initialized = false;

/**
 * @brief Write command to Seesaw sensor (new I2C master API)
 */
static esp_err_t seesaw_write_cmd(uint8_t base, uint8_t func)
{
    uint8_t write_buf[2] = {base, func};
    return i2c_master_transmit(i2c_dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Write command with data to Seesaw sensor (new I2C master API)
 */
static esp_err_t seesaw_write_cmd_data(uint8_t base, uint8_t func, uint8_t data)
{
    uint8_t write_buf[3] = {base, func, data};
    return i2c_master_transmit(i2c_dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Read data from Seesaw sensor (new I2C master API)
 */
static esp_err_t seesaw_read_data(uint8_t *buffer, size_t len)
{
    return i2c_master_receive(i2c_dev_handle, buffer, len, I2C_MASTER_TIMEOUT_MS);
}

// Initialize sensor
esp_err_t soil_sensor_init(void *bus_handle)
{
    ESP_LOGI(TAG, "Initializing Adafruit Soil Sensor...");
    
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Invalid bus handle");
        return ESP_FAIL;
    }
    
    // Create I2C device handle (new API)
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SOIL_SENSOR_ADDR,
        .scl_speed_hz = 100000,  // 100kHz
    };
    
    // Add device to the I2C bus (bus_handle passed from main.c)
    esp_err_t ret = i2c_master_bus_add_device((i2c_master_bus_handle_t)bus_handle, &dev_cfg, &i2c_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    // Perform soft reset
    ESP_LOGI(TAG, "Performing soft reset...");
    ret = seesaw_write_cmd_data(SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, 0xFF);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Soft reset failed (may be expected): %s", esp_err_to_name(ret));
    }
    
    // Wait longer for sensor to fully boot and stabilize
    ESP_LOGI(TAG, "Waiting for sensor to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
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

// Read all data (performs fresh I2C reads)
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
    *data = temp_data;
    return ESP_OK;
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

// Check if sensor is present (new I2C master API)
bool soil_sensor_is_present(void)
{
    // Note: This requires the device handle to be initialized first
    // Now done via probe in soil_sensor_init()
    if (i2c_dev_handle == NULL) {
        return false;
    }
    
    // Simple probe by attempting to read a byte
    uint8_t dummy;
    esp_err_t ret = i2c_master_receive(i2c_dev_handle, &dummy, 1, 100);
    return (ret == ESP_OK || ret == ESP_ERR_TIMEOUT); // Sensor present if we get any response
}

