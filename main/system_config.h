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
#define ZIGBEE_TASK_STACK      8192  // DOUBLED - Zigbee stack needs more space

// Task Priorities
#define MONITORING_TASK_PRIORITY 5
#define ZIGBEE_TASK_PRIORITY    6

#endif // SYSTEM_CONFIG_H

