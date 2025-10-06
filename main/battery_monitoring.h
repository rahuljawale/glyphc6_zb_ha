/*
 * Glyph C6 - Battery Monitoring Module
 * 
 * Version: 1.0.0
 * 
 * ADC-based battery monitoring using GPIO12 (BATT_MSR)
 * Voltage divider: 200kΩ/200kΩ (2:1 ratio)
 * Reports battery voltage and percentage via Zigbee Power Config cluster
 */

#ifndef BATTERY_MONITORING_H
#define BATTERY_MONITORING_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize battery monitoring system
 * Sets up ADC on GPIO12 for battery voltage sensing
 * 
 * @return ESP_OK on success
 */
esp_err_t battery_monitoring_init(void);

/**
 * @brief Start battery monitoring task
 * Reads battery voltage periodically and updates cache
 * 
 * @return ESP_OK on success
 */
esp_err_t battery_monitoring_start_task(void);

/**
 * @brief Get cached battery data (thread-safe)
 * 
 * @param voltage Output: battery voltage in volts
 * @param percentage Output: battery percentage (0-100%)
 * @return true if cache is valid, false if stale
 */
bool battery_get_cached_data(float *voltage, float *percentage);

/**
 * @brief Detect if USB power is present
 * Uses battery voltage behavior to infer charging status
 * 
 * @return true if USB power detected (charging or full)
 */
bool battery_is_usb_present(void);

#endif // BATTERY_MONITORING_H
