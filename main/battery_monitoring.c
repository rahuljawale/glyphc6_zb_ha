/*
 * Glyph C6 - Battery Monitoring Module Implementation
 * Simplified for deep sleep mode - direct reads only, no background tasks
 * 
 * Version: 1.0.0
 * 
 * ADC-based battery monitoring for LiPo battery via GPIO12
 */

#include "battery_monitoring.h"
#include "system_config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "BATTERY_MON";

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================

// ADC handles
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

// ============================================================================
// PRIVATE FUNCTIONS
// ============================================================================

/**
 * @brief Convert LiPo voltage to percentage using discharge curve
 * Based on typical LiPo discharge characteristics
 */
static float voltage_to_percentage(float voltage)
{
    // USB power detection: If voltage > 4.3V, USB is connected
    // In this case, we can't accurately measure battery percentage
    // Return 100% to indicate "powered/charging"
    if (voltage > 4.3f) {
        return 100.0f;  // USB connected - report as fully powered
    }
    
    // LiPo discharge curve (simplified) - only valid when on battery
    // 4.2V = 100%, 3.7V = 50%, 3.0V = 0%
    
    if (voltage >= BATT_VOLTAGE_MAX) {
        return 100.0f;
    } else if (voltage <= BATT_VOLTAGE_MIN) {
        return 0.0f;
    }
    
    // Piecewise linear approximation of LiPo curve
    if (voltage > 3.9f) {
        // 3.9V-4.2V: 80-100% (steep region)
        return 80.0f + ((voltage - 3.9f) / 0.3f) * 20.0f;
    } else if (voltage > 3.7f) {
        // 3.7V-3.9V: 50-80% (linear region)
        return 50.0f + ((voltage - 3.7f) / 0.2f) * 30.0f;
    } else if (voltage > 3.4f) {
        // 3.4V-3.7V: 20-50% (linear region)
        return 20.0f + ((voltage - 3.4f) / 0.3f) * 30.0f;
    } else {
        // 3.0V-3.4V: 0-20% (steep discharge)
        return ((voltage - BATT_VOLTAGE_MIN) / 0.4f) * 20.0f;
    }
}

/**
 * @brief Read battery voltage from ADC
 * Averages multiple samples for stability
 */
static esp_err_t read_battery_voltage(float *voltage)
{
    if (!adc_handle || !voltage) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int total_mv = 0;
    int valid_samples = 0;
    
    // Take multiple samples and average
    for (int i = 0; i < BATTERY_SAMPLES_AVG; i++) {
        int adc_raw;
        esp_err_t ret = adc_oneshot_read(adc_handle, BATT_MSR_ADC_CHANNEL, &adc_raw);
        
        if (ret == ESP_OK) {
            int adc_mv;
            if (adc_cali_handle) {
                ret = adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &adc_mv);
                if (ret == ESP_OK) {
                    total_mv += adc_mv;
                    valid_samples++;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Small delay between samples
    }
    
    if (valid_samples == 0) {
        return ESP_FAIL;
    }
    
    // Calculate average and convert to battery voltage
    int avg_mv = total_mv / valid_samples;
    *voltage = BATT_ADC_TO_VOLTAGE(avg_mv);
    
    // Debug output
    ESP_LOGI(TAG, "ADC Debug: raw_avg=%d mV, after_divider=%.2fV (divider=%.2f)", 
             avg_mv, *voltage, BATT_VOLTAGE_DIVIDER);
    
    return ESP_OK;
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

esp_err_t battery_monitoring_init(void)
{
    ESP_LOGI(TAG, "Initializing battery monitoring (GPIO0/A0, ADC1_CH0)...");
    
    // Configure ADC
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = BATT_MSR_ADC_UNIT,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&adc_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure channel
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = BATT_MSR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    ret = adc_oneshot_config_channel(adc_handle, BATT_MSR_ADC_CHANNEL, &chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATT_MSR_ADC_UNIT,
        .atten = BATT_MSR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration initialized");
    } else {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
        adc_cali_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Battery monitoring hardware initialized successfully");
    return ESP_OK;
}

esp_err_t battery_read(float *voltage, float *percentage)
{
    if (!voltage || !percentage) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read voltage directly from ADC
    esp_err_t ret = read_battery_voltage(voltage);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Convert to percentage
    *percentage = voltage_to_percentage(*voltage);
    
    return ESP_OK;
}

bool battery_is_usb_present(void)
{
    float voltage = 0.0f;
    float percentage = 0.0f;
    
    // Read directly from ADC
    if (battery_read(&voltage, &percentage) != ESP_OK) {
        return false;  // Unknown state
    }
    
    // USB present if voltage > 4.3V (USB power is ~4.7V, battery max is 4.2V)
    return (voltage > 4.3f);
}
