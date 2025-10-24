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
 * @brief Read battery voltage and calculate percentage (direct ADC read)
 * 
 * This function directly reads from the ADC hardware.
 * Suitable for deep sleep mode where sensors are read on-demand.
 * 
 * @param voltage Output: battery voltage in volts
 * @param percentage Output: battery percentage (0-100%)
 * @return ESP_OK on success, ESP_FAIL on ADC read error
 */
esp_err_t battery_read(float *voltage, float *percentage);

/**
 * @brief Detect if USB power is present (performs direct ADC read)
 * Uses battery voltage behavior to infer charging status
 * 
 * @return true if USB power detected (charging or full)
 */
bool battery_is_usb_present(void);

#endif // BATTERY_MONITORING_H
