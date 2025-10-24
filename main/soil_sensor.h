/*
 * Adafruit STEMMA Soil Sensor (4026) Driver
 * 
 * I2C-based capacitive soil moisture sensor with temperature measurement
 * Uses Adafruit Seesaw protocol (ATSAMD10 microcontroller)
 */

#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Soil sensor data structure
typedef struct {
    uint16_t moisture_raw;        // Raw capacitance value (200-2000)
    float moisture_percent;       // Moisture percentage (0-100%)
    float temperature_c;          // Temperature in Celsius
    float temperature_f;          // Temperature in Fahrenheit
    bool valid;                   // Data validity flag
    uint32_t timestamp;           // Last reading timestamp (ms)
} soil_data_t;

// Moisture status enum
typedef enum {
    SOIL_STATUS_CRITICAL = 0,     // < 20% - Water NOW!
    SOIL_STATUS_LOW,              // 20-35% - Water soon
    SOIL_STATUS_GOOD,             // 35-65% - Happy plant
    SOIL_STATUS_HIGH,             // 65-85% - Don't water
    SOIL_STATUS_SATURATED,        // > 85% - Too wet
    SOIL_STATUS_ERROR             // Read error
} soil_status_t;

/**
 * @brief Initialize soil sensor
 * 
 * Performs I2C check and soft reset of sensor
 * 
 * @param bus_handle I2C master bus handle (from i2c_new_master_bus)
 * @return ESP_OK on success, ESP_FAIL if sensor not found
 */
esp_err_t soil_sensor_init(void *bus_handle);

/**
 * @brief Read soil moisture (capacitance)
 * 
 * @param raw_value Pointer to store raw value (200-2000)
 * @param percent Pointer to store percentage (0-100%)
 * @return ESP_OK on success
 */
esp_err_t soil_sensor_read_moisture(uint16_t *raw_value, float *percent);

/**
 * @brief Read soil temperature
 * 
 * @param temp_c Pointer to store temperature in Celsius
 * @param temp_f Pointer to store temperature in Fahrenheit
 * @return ESP_OK on success
 */
esp_err_t soil_sensor_read_temperature(float *temp_c, float *temp_f);

/**
 * @brief Read all sensor data at once (performs fresh I2C reads)
 * 
 * This function directly reads from the sensor hardware.
 * Suitable for deep sleep mode where sensors are read on-demand.
 * 
 * @param data Pointer to soil_data_t structure
 * @return ESP_OK on success
 */
esp_err_t soil_sensor_read_all(soil_data_t *data);

/**
 * @brief Determine moisture status based on thresholds
 * 
 * @param percent Moisture percentage (0-100%)
 * @return soil_status_t enum value
 */
soil_status_t soil_sensor_get_status(float percent);

/**
 * @brief Get status string for logging
 * 
 * @param status soil_status_t value
 * @return String representation with emoji
 */
const char* soil_sensor_status_string(soil_status_t status);

/**
 * @brief Check if soil sensor is present on I2C bus
 * 
 * @return true if sensor responds at 0x36
 */
bool soil_sensor_is_present(void);

#endif // SOIL_SENSOR_H

