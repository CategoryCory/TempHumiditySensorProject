// wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and connect to Wi-Fi in station mode.
 * Blocks until connection is established or fails after a set number of retries.
 * 
 * @return ESP_OK on successful connection, ESP_FAIL otherwise.
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Check if the device is currently connected to Wi-Fi.
 * 
 * @return true if connected, false otherwise.
 */
bool wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H