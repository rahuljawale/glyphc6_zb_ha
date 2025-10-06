/*
 * Glyph C6 Monitor - System Configuration
 * 
 * Board: ESP32-C6-MINI-1 (Adafruit ESP32-C6 Feather pinout)
 * Version: 1.0.0
 * 
 * This file contains all system constants, pin definitions,
 * thresholds, and configuration parameters.
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS (Glyph C6 / Adafruit ESP32-C6 Feather)
// ============================================================================

// LED Configuration
#define LED_PIN                 GPIO_NUM_15     // Red LED
#define NEOPIXEL_I2C_POWER      GPIO_NUM_20     // Power for NeoPixel and I2C
#define NEOPIXEL_PIN            GPIO_NUM_9      // NeoPixel (shared with boot button)

// I2C Configuration (STEMMA QT connector)
#define I2C_SDA_PIN             GPIO_NUM_19
#define I2C_SCL_PIN             GPIO_NUM_18
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      100000          // 100kHz
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS   1000

// UART Configuration
#define UART_TX_PIN             GPIO_NUM_16
#define UART_RX_PIN             GPIO_NUM_17

// SPI Configuration
#define SPI_SCK_PIN             GPIO_NUM_21
#define SPI_MOSI_PIN            GPIO_NUM_22
#define SPI_MISO_PIN            GPIO_NUM_23

// ADC Configuration
#define ADC_A0                  GPIO_NUM_0
#define ADC_A1                  GPIO_NUM_1
#define ADC_A2                  GPIO_NUM_6      // Shared with IO6
#define ADC_A3                  GPIO_NUM_5      // Shared with IO5
#define ADC_A4                  GPIO_NUM_3
#define ADC_A5                  GPIO_NUM_4

// ============================================================================
// BATTERY MONITORING CONFIGURATION (from Glyph C6 schematic)
// ============================================================================

// Battery ADC Configuration
// GPIO_12 (BATT_MSR) - from schematic header section
#define BATT_MSR_GPIO           GPIO_NUM_12
#define BATT_MSR_ADC_UNIT       ADC_UNIT_1
#define BATT_MSR_ADC_CHANNEL    ADC_CHANNEL_0     // GPIO12 = ADC1_CH0 on ESP32-C6
#define BATT_MSR_ADC_ATTEN      ADC_ATTEN_DB_12   // 0-3.3V range
#define BATT_MSR_ADC_BITWIDTH   ADC_BITWIDTH_12

// Battery Voltage Divider (from schematic: R10=200kΩ, R11=200kΩ)
// BATT → R10 (200k) → BATT_MSR (to ADC) → R11 (200k) → GND
#define BATT_R1                 200000.0f         // 200kΩ upper resistor (R10)
#define BATT_R2                 200000.0f         // 200kΩ lower resistor (R11)
#define BATT_VOLTAGE_DIVIDER    ((BATT_R1 + BATT_R2) / BATT_R2)  // 2.0x
#define BATT_ADC_TO_VOLTAGE(mv) ((mv / 1000.0f) * BATT_VOLTAGE_DIVIDER)

// Battery Voltage Thresholds (LiPo)
#define BATT_VOLTAGE_MAX        4.2f              // Fully charged
#define BATT_VOLTAGE_MIN        3.0f              // Empty/cutoff
#define BATT_VOLTAGE_LOW        3.4f              // Low battery warning

// Battery Sampling
#define BATTERY_SAMPLES_AVG     10                // Number of ADC samples to average
#define BATTERY_READ_INTERVAL   60000             // 60 seconds between reads

// Battery Thresholds
#define BATTERY_LOW_PERCENT     20.0f             // Below this = low battery
#define BATTERY_FULL_PERCENT    99.0f             // Above this = full

// ============================================================================
// ZIGBEE CONFIGURATION
// ============================================================================

// Device Information
#define ESP_MANUFACTURER_NAME    "\x09""ESPRESSIF"
#define ESP_MODEL_IDENTIFIER     "\x0B""GLYPH_C6_M1"  // Glyph C6 Monitor v1

// Zigbee Network Configuration
#define INSTALLCODE_POLICY_ENABLE false
#define ED_AGING_TIMEOUT         ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE           3000              // 3000 milliseconds
#define HA_ESP_SENSOR_ENDPOINT  1                 // Main endpoint
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK

// Reporting Intervals
#define ZIGBEE_REPORT_INTERVAL  30000             // 30 seconds between reports

// ============================================================================
// TASK CONFIGURATION
// ============================================================================

// Task Stack Sizes (INCREASED for stability)
#define MONITORING_TASK_STACK   4096
#define BATTERY_TASK_STACK      4096  // INCREASED - ADC + logging needs more space
#define ZIGBEE_TASK_STACK       8192  // DOUBLED - Zigbee stack needs more space

// Task Priorities
#define MONITORING_TASK_PRIORITY 5
#define BATTERY_TASK_PRIORITY    4
#define ZIGBEE_TASK_PRIORITY     6

// ============================================================================
// THREAD SAFETY CONFIGURATION
// ============================================================================

// Mutex Timeout Values
#define BATTERY_MUTEX_TIMEOUT_MS 100

#endif // SYSTEM_CONFIG_H

