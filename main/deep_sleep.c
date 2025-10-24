/*
 * Glyph C6 Monitor - Deep Sleep Management Module
 * 
 * Version: 1.0.0
 */

#include "deep_sleep.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DEEP_SLEEP";

// ============================================================================
// RTC MEMORY (persists across deep sleep)
// ============================================================================

// RTC_DATA_ATTR stores variables in RTC slow memory which persists during deep sleep
static RTC_DATA_ATTR deep_sleep_state_t rtc_state = {
    .boot_count = 0,
    .sensor_read_count = 0,
    .last_read_time = 0,
    .first_boot = true,
};

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================

static bool initialized = false;
static uint64_t wake_time_us = 0;  // Time when device woke up (microseconds)

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

esp_err_t deep_sleep_init(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  Deep Sleep Manager - Ultra Power Saving");
    ESP_LOGI(TAG, "===========================================");
    
    // Get wake time
    wake_time_us = esp_timer_get_time();
    
    // Increment boot count
    rtc_state.boot_count++;
    
    // Get wake cause
    esp_sleep_wakeup_cause_t wake_cause = esp_sleep_get_wakeup_cause();
    
    switch (wake_cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Wake from timer (boot #%lu)", rtc_state.boot_count);
        rtc_state.first_boot = false;
        break;
        
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        if (rtc_state.boot_count == 1) {
            ESP_LOGI(TAG, "First boot after power-on");
            rtc_state.first_boot = true;
            rtc_state.last_read_time = 0;
        } else {
            ESP_LOGI(TAG, "Wake from reset or other cause");
        }
        break;
    }
    
    initialized = true;
    deep_sleep_print_stats();
    
    return ESP_OK;
}

bool deep_sleep_get_state(deep_sleep_state_t *state)
{
    if (!initialized || !state) {
        return false;
    }
    
    *state = rtc_state;
    return true;
}

bool deep_sleep_should_read_sensors(void)
{
    if (!initialized) {
        return false;
    }
    
    // Always read on first boot
    if (rtc_state.first_boot) {
        ESP_LOGI(TAG, "First boot - sensors will be read");
        return true;
    }
    
    // Calculate time since last reading
    uint64_t time_since_read_us = wake_time_us - rtc_state.last_read_time;
    uint64_t read_interval_us = (uint64_t)SLEEP_INTERVAL_SEC * 1000000ULL;
    
    // Read if interval has elapsed
    bool should_read = time_since_read_us >= read_interval_us;
    
    ESP_LOGI(TAG, "Sensor check: %lld us since last read, interval: %lld us -> %s",
             time_since_read_us, read_interval_us, should_read ? "READ" : "SKIP");
    
    return should_read;
}

void deep_sleep_mark_sensors_read(void)
{
    rtc_state.last_read_time = wake_time_us;
    rtc_state.sensor_read_count++;
    ESP_LOGI(TAG, "Sensors reading marked (total: %lu)", rtc_state.sensor_read_count);
}

uint32_t deep_sleep_time_until_next_reading(void)
{
    if (!initialized) {
        return SLEEP_INTERVAL_SEC;
    }
    
    uint64_t time_since_read_us = wake_time_us - rtc_state.last_read_time;
    uint64_t read_interval_us = (uint64_t)SLEEP_INTERVAL_SEC * 1000000ULL;
    
    if (time_since_read_us >= read_interval_us) {
        return 0;  // Reading is due now
    }
    
    uint64_t time_until_read_us = read_interval_us - time_since_read_us;
    return (uint32_t)(time_until_read_us / 1000000ULL);
}

void deep_sleep_print_stats(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Deep Sleep Statistics:");
    ESP_LOGI(TAG, "  Boot count:         %lu", rtc_state.boot_count);
    ESP_LOGI(TAG, "  Sensor readings:    %lu", rtc_state.sensor_read_count);
    ESP_LOGI(TAG, "  First boot:         %s", rtc_state.first_boot ? "YES" : "NO");
    ESP_LOGI(TAG, "  Read interval:      %d seconds (1 hour)", SLEEP_INTERVAL_SEC);
    
    if (!rtc_state.first_boot) {
        uint32_t next_read_sec = deep_sleep_time_until_next_reading();
        ESP_LOGI(TAG, "  Next reading:       %lu seconds (%.1f hours)", 
                 next_read_sec, next_read_sec / 3600.0f);
    }
    ESP_LOGI(TAG, "");
}

esp_err_t deep_sleep_enter(void)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Deep sleep not initialized!");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  Preparing for Deep Sleep");
    ESP_LOGI(TAG, "===========================================");
    
    // Calculate next wake time (simple: always 1 hour)
    uint32_t sleep_duration_sec = SLEEP_INTERVAL_SEC;
    
    // Minimum sleep time is 10 seconds (avoid rapid wake/sleep cycles during testing)
    if (sleep_duration_sec < 10) {
        sleep_duration_sec = 10;
    }
    
    ESP_LOGI(TAG, "Sleep duration: %lu seconds (%.1f hours)", 
             sleep_duration_sec, sleep_duration_sec / 3600.0f);
    ESP_LOGI(TAG, "Next wake: Soil + Battery readings together");
    
    // Clear first boot flag
    rtc_state.first_boot = false;
    
    // Configure wake-up timer
    uint64_t sleep_duration_us = (uint64_t)sleep_duration_sec * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleep_duration_us);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💤 Entering deep sleep... See you in %.1f hours!", sleep_duration_sec / 3600.0f);
    ESP_LOGI(TAG, "===========================================");
    
    // Small delay to ensure log is flushed
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Enter deep sleep (device will reset on wake)
    esp_deep_sleep_start();
    
    // Never reached
    return ESP_OK;
}

