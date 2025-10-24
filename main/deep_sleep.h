/*
 * Glyph C6 Monitor - Deep Sleep Management Module
 * 
 * Version: 1.0.0
 * 
 * Manages deep sleep cycles for extreme battery preservation.
 * Uses ESP32-C6 deep sleep with Zigbee Sleepy End Device mode.
 * 
 * Battery Life Estimate:
 * - With 1000mAh battery: 10-16 months
 * - Deep sleep current: ~10µA
 * - Wake/read/transmit: ~50mA for 1 minute per hour
 */

#ifndef DEEP_SLEEP_H
#define DEEP_SLEEP_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ============================================================================
// DEEP SLEEP CONFIGURATION
// ============================================================================

// Sleep intervals (in seconds)
// Both soil and battery read at the same time for efficiency
#define SLEEP_INTERVAL_SEC           3600     // 1 hour between readings (soil + battery)

// Wake behavior
#define WAKE_TIME_MS                 60000    // Stay awake for 1 minute to read/transmit
#define ZIGBEE_POLL_TIME_MS          5000     // Poll Zigbee for 5 seconds after wake

// Sensor averaging
#define NUM_SENSOR_SAMPLES           5        // Take 5 samples and average
#define SAMPLE_INTERVAL_MS           5000     // 5 seconds between samples (25s total)

// OTA behavior
#define OTA_CHECK_ENABLED            1        // Check for OTA on wake
#define OTA_DOWNLOAD_TIMEOUT_MS      300000   // 5 minutes max for OTA download

// Boot count tracking (in RTC memory - persists across deep sleep)
typedef struct {
    uint32_t boot_count;              // Total number of boots
    uint32_t sensor_read_count;       // Number of sensor readings (soil + battery)
    uint64_t last_read_time;          // Last reading timestamp (us since boot)
    bool first_boot;                  // First boot after power-on
} deep_sleep_state_t;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize deep sleep management system
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t deep_sleep_init(void);

/**
 * @brief Get current deep sleep state (from RTC memory)
 * @param state Pointer to store state
 * @return true if state is valid, false otherwise
 */
bool deep_sleep_get_state(deep_sleep_state_t *state);

/**
 * @brief Check if sensor readings are due this wake cycle
 * Both soil and battery are read together every 1 hour
 * @return true if sensors should be read, false otherwise
 */
bool deep_sleep_should_read_sensors(void);

/**
 * @brief Mark sensor readings as completed
 * Call this after successfully reading both soil and battery
 */
void deep_sleep_mark_sensors_read(void);

/**
 * @brief Enter deep sleep mode
 * 
 * This function calculates the next wake time based on pending tasks,
 * prepares the Zigbee stack for sleep, and enters deep sleep.
 * Device will wake up after the calculated interval.
 * 
 * @return ESP_OK if sleep was successful (never returns), error otherwise
 */
esp_err_t deep_sleep_enter(void);

/**
 * @brief Get time until next sensor readings (seconds)
 * @return Seconds until next readings (soil + battery)
 */
uint32_t deep_sleep_time_until_next_reading(void);

/**
 * @brief Print deep sleep statistics
 */
void deep_sleep_print_stats(void);

#endif // DEEP_SLEEP_H

