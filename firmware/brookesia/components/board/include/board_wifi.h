/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "adf_event_hub.h"
#include "esp_wifi_service_profile_mgr.h"
#include "esp_wifi_service_scan.h"
#include "esp_wifi_service_selector.h"

/*
 * esp_service.h (and therefore esp_wifi_service.h) declares a C11 `_Atomic` struct member and cannot
 * be included from C++. This header only uses the C++-safe Wi-Fi service headers and wraps the
 * service calls applications need.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * Board Wi-Fi interface
 *
 * The board owns one esp_wifi_service instance. It initializes esp_netif, the default event loop,
 * the STA netif and the Wi-Fi driver, and keeps the saved profiles in NVS. Applications should not
 * call esp_wifi_init()/esp_wifi_start() themselves; they subscribe to the service events instead:
 *
 *     board_wifi_service_subscribe(BOARD_WIFI_EVENT_STA_GOT_IP, my_handler, my_ctx);
 *
 * Event handlers run synchronously in the publisher task (esp_event loop or selector task). They
 * must not block, must not touch LVGL directly and must not call Wi-Fi service APIs that publish
 * events. The event hub can only unsubscribe by event id, which would also drop the subscriptions
 * of other applications, so subscriptions are kept for the lifetime of the application.
 **************************************************************************************************/

/**
 * @brief Wi-Fi service event ids (same values as esp_wifi_service_event_t)
 *
 * Payloads: CONNECTED -> wifi_event_sta_connected_t, DISCONNECTED -> wifi_event_sta_disconnected_t,
 * STA_CONFIG -> esp_wifi_service_sta_config_event_t (published right before each connect attempt),
 * STA_GOT_IP -> ip_event_got_ip_t, STA_LOST_IP -> none.
 */
typedef enum {
    BOARD_WIFI_EVENT_CONNECTED              = 101,
    BOARD_WIFI_EVENT_DISCONNECTED           = 102,
    BOARD_WIFI_EVENT_STA_CONFIG             = 110,
    BOARD_WIFI_EVENT_STA_GOT_IP             = 111,
    BOARD_WIFI_EVENT_STA_LOST_IP            = 112,
    BOARD_WIFI_EVENT_SELECTOR_SWITCH_FAILED = 115,
    BOARD_WIFI_EVENT_ANY                    = ADF_EVENT_ANY_ID,
} board_wifi_event_t;

/**
 * @brief Station link state tracked by the board from boot
 */
typedef enum {
    BOARD_WIFI_LINK_IDLE = 0,      /*!< Not connected and no attempt in progress */
    BOARD_WIFI_LINK_CONNECTING,    /*!< A connect attempt to `ssid` is in progress */
    BOARD_WIFI_LINK_ASSOCIATED,    /*!< Associated with `ssid`, waiting for an IP address */
    BOARD_WIFI_LINK_GOT_IP,        /*!< Connected to `ssid` with an IP address */
} board_wifi_link_state_t;

/**
 * @brief Snapshot of the station link
 */
typedef struct {
    board_wifi_link_state_t state;  /*!< Link state */
    char ssid[33];                      /*!< SSID of the attempt or connection; empty when idle */
} board_wifi_link_t;

/**
 * @brief Create the board Wi-Fi service and start it when Wi-Fi is enabled in NVS
 *
 * @note NVS must be initialized before calling this function. Calling it again is a no-op.
 *
 * @return
 *     - ESP_OK: Service is ready
 *     - Others: Failed to create the profile storage or the Wi-Fi service
 */
esp_err_t board_wifi_service_init(void);

/**
 * @brief Check whether the board Wi-Fi service has been created
 */
bool board_wifi_service_is_ready(void);

/** @brief ESP-Hosted Wi-Fi co-processor identity */
typedef struct {
    const char *chip;    /*!< Co-processor target from the esp_hosted configuration, such as "esp32c6" */
    bool        valid;   /*!< The firmware version below was read from the co-processor */
    uint32_t    major;   /*!< Co-processor ESP-Hosted firmware version */
    uint32_t    minor;
    uint32_t    patch;
} board_wifi_coprocessor_info_t;

/**
 * @brief Get the Wi-Fi co-processor chip and its ESP-Hosted firmware version
 *
 * The version is read once over the hosted link by board_wifi_service_init() and cached, so this
 * call never blocks; valid stays false before that or when the co-processor did not answer.
 *
 * @param[out] info Co-processor information
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: info is NULL
 */
esp_err_t board_wifi_get_coprocessor_info(board_wifi_coprocessor_info_t *info);

/**
 * @brief Subscribe to Wi-Fi service events
 *
 * @note May be called before board_wifi_service_init(); events are delivered once the service runs.
 *
 * @param event_id Event id from board_wifi_event_t, or BOARD_WIFI_EVENT_ANY
 * @param handler Callback invoked in the publisher task
 * @param ctx User context forwarded to the handler
 * @return
 *     - ESP_OK: Subscribed
 *     - Others: Event hub error
 */
esp_err_t board_wifi_service_subscribe(uint16_t event_id, adf_event_handler_t handler, void *ctx);

/**
 * @brief Get the profile manager shared by the board Wi-Fi service
 *
 * @return Profile manager handle, or NULL if the service is not initialized
 */
esp_wifi_service_profile_mgr_t board_wifi_service_get_profile_manager(void);

/**
 * @brief Get the scan agent shared by the board Wi-Fi service
 *
 * @return Scan handle, or NULL if the service is not initialized
 */
esp_wifi_service_scan_handle_t board_wifi_service_get_scan_handle(void);

/**
 * @brief Save a profile and ask the service to connect to it (see esp_wifi_service_request_connect)
 *
 * @param ssid NUL-terminated SSID
 * @param password NUL-terminated password; NULL or "" for open networks
 * @param priority Profile priority, 0 to ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX
 * @param wait_sec Seconds to wait for an IP address; 0 returns once the request is accepted
 */
esp_err_t board_wifi_service_request_connect(const char *ssid, const char *password, uint8_t priority,
                                                 uint32_t wait_sec);

/**
 * @brief Ask the service to re-evaluate saved profiles (see esp_wifi_service_request_reeval)
 */
esp_err_t board_wifi_service_request_reeval(void);

/**
 * @brief Enable or disable Wi-Fi and persist the choice in NVS
 *
 * Enabling starts the service, which connects to the best saved profile. Disabling stops the
 * service and disconnects the station.
 *
 * @param enable true to enable Wi-Fi
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_STATE: Service is not initialized
 *     - Others: Failed to start or stop the service
 */
esp_err_t board_wifi_service_set_enabled(bool enable);

/**
 * @brief Check whether Wi-Fi is enabled by the user
 */
bool board_wifi_service_is_enabled(void);

/**
 * @brief Get the current station link state, including an attempt that is still connecting
 *
 * @note The state is tracked from board_wifi_service_init(), so pages opened later still see an
 *       automatic connection that is in progress. Safe to call from any task.
 *
 * @param[out] link Link snapshot
 */
void board_wifi_service_get_link(board_wifi_link_t *link);

/**
 * @brief Check whether the station currently has an IPv4 address
 */
bool board_wifi_service_is_connected(void);

#ifdef __cplusplus
}
#endif
