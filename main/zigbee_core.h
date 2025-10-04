/*
 * Glyph C6 Monitor - Zigbee Core Module
 * 
 * Version: 1.0.0
 * 
 * Handles Zigbee stack initialization, cluster creation, device setup,
 * network management, and main loop processing.
 */

#ifndef ZIGBEE_CORE_H
#define ZIGBEE_CORE_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_endpoint.h"
#include "system_config.h"

// ============================================================================
// ZIGBEE CORE PUBLIC INTERFACE
// ============================================================================

/**
 * @brief Zigbee device configuration structure
 */
typedef struct {
    bool zigbee_joined;          // Network join status
    uint32_t last_zigbee_report; // Last report timestamp
    uint16_t pan_id;             // Network PAN ID
    uint8_t channel;             // Current network channel
    uint16_t short_address;      // Device short address
} zigbee_device_info_t;

/**
 * @brief Initialize Zigbee core system
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_init(void);

/**
 * @brief Deinitialize Zigbee core system
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_deinit(void);

/**
 * @brief Start Zigbee stack and begin commissioning
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_start(void);

/**
 * @brief Stop Zigbee stack
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_stop(void);

/**
 * @brief Start Zigbee main loop task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_start_main_loop_task(void);

/**
 * @brief Stop Zigbee main loop task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_stop_main_loop_task(void);

/**
 * @brief Get Zigbee device information
 * @param info Pointer to store device information
 * @return true if data valid, false otherwise
 */
bool zigbee_core_get_device_info(zigbee_device_info_t *info);

/**
 * @brief Check if Zigbee network is joined
 * @return true if joined, false otherwise
 */
bool zigbee_core_is_joined(void);

/**
 * @brief Set initial attribute values for device
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_set_initial_attributes(void);

/**
 * @brief Create custom sensor clusters for the device
 * @param basic_cfg Basic cluster configuration
 * @param identify_cfg Identify cluster configuration
 * @return Cluster list or NULL on error
 */
esp_zb_cluster_list_t *zigbee_core_create_sensor_clusters(
    esp_zb_basic_cluster_cfg_t *basic_cfg, 
    esp_zb_identify_cluster_cfg_t *identify_cfg);

/**
 * @brief Create custom sensor endpoint
 * @param endpoint_id Endpoint identifier
 * @param basic_cfg Basic cluster configuration
 * @param identify_cfg Identify cluster configuration
 * @return Endpoint list or NULL on error
 */
esp_zb_ep_list_t *zigbee_core_create_sensor_endpoint(
    uint8_t endpoint_id,
    esp_zb_basic_cluster_cfg_t *basic_cfg, 
    esp_zb_identify_cluster_cfg_t *identify_cfg);

/**
 * @brief Zigbee application signal handler (callback)
 * @param signal_struct Signal structure from Zigbee stack
 */
void zigbee_core_app_signal_handler(esp_zb_app_signal_t *signal_struct);

/**
 * @brief Register action handler callback
 * @param handler_func Action handler function pointer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_core_register_action_handler(esp_err_t (*handler_func)(esp_zb_core_action_callback_id_t, const void *));

#endif // ZIGBEE_CORE_H

