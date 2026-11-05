/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "esp_check.h"
#include "esp_config_storage.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "esp_service.h"
#include "esp_wifi_service.h"
#include "esp_hosted.h"

#include "board_wifi.h"

_Static_assert((int)BOARD_WIFI_EVENT_CONNECTED == (int)ESP_WIFI_SERVICE_EVENT_CONNECTED, "Wi-Fi event id mismatch");
_Static_assert((int)BOARD_WIFI_EVENT_DISCONNECTED == (int)ESP_WIFI_SERVICE_EVENT_DISCONNECTED, "Wi-Fi event id mismatch");
_Static_assert((int)BOARD_WIFI_EVENT_STA_CONFIG == (int)ESP_WIFI_SERVICE_EVENT_STA_CONFIG, "Wi-Fi event id mismatch");
_Static_assert((int)BOARD_WIFI_EVENT_STA_GOT_IP == (int)ESP_WIFI_SERVICE_EVENT_STA_GOT_IP, "Wi-Fi event id mismatch");
_Static_assert((int)BOARD_WIFI_EVENT_STA_LOST_IP == (int)ESP_WIFI_SERVICE_EVENT_STA_LOST_IP, "Wi-Fi event id mismatch");
_Static_assert((int)BOARD_WIFI_EVENT_SELECTOR_SWITCH_FAILED == (int)ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCH_FAILED, "Wi-Fi event id mismatch");

#define WIFI_SERVICE_NAME           "wifi_service"
#define WIFI_PROFILE_MAX_NUM        (8)
#define WIFI_STA_NETIF_KEY          "WIFI_STA_DEF"

/* Keep the key used by the previous Settings implementation so the user's switch state survives. */
#define WIFI_ENABLE_NVS_NAMESPACE   "storage"
#define WIFI_ENABLE_NVS_KEY         "wifi_en"

static const char *TAG = "board_wifi";

static const esp_config_storage_nvs_t s_profile_store = {
    .nvs_namespace = "wifi_svc",
    .key_primary   = "prof_p",
    .key_backup    = "prof_b",
};

/*
 * Mirrors the selector's built-in defaults (esp_wifi_service 0.5.3), which are not exported,
 * except that automatic reconnection gives up after a few consecutive failures.
 */
static const esp_wifi_service_selector_cfg_t s_selector_policy = {
    .triggers_mask = ESP_WIFI_SERVICE_SELECTOR_TRIGGER_RSSI_LOW,
    .select_order = {
        ESP_WIFI_SERVICE_SELECTOR_CRITERION_PROBE_TRUSTED,
        ESP_WIFI_SERVICE_SELECTOR_CRITERION_QUALITY,
        ESP_WIFI_SERVICE_SELECTOR_CRITERION_PRIORITY,
    },
    .rssi = {
        .threshold_dbm = -75,
        .check_period_ms = 5000,
    },
    .probe = {
        .url = NULL,
        .check_period_min = 5,
        .timeout_ms = 5000,
        .expected_status = 204,
        .blocked_seconds = 15,
        .task = {
            .task_core = -1,
        },
    },
    .throughput = {
        .task = {
            .task_core = -1,
        },
    },
    .retry = {
        .scan_retry_ms = NULL,      /* NULL with scan_retry_num 0 keeps the built-in scan backoff */
        .scan_retry_num = 0,
        .max_connect_retry = 3,
    },
};

static esp_config_storage_t s_profile_storage;
static esp_wifi_service_profile_mgr_t s_profile_mgr;
static esp_wifi_service_t *s_wifi_service;
static adf_event_hub_t s_wifi_event_hub;
static SemaphoreHandle_t s_wifi_lock;
static bool s_wifi_enabled;
static board_wifi_link_t s_link;
static portMUX_TYPE s_link_lock = portMUX_INITIALIZER_UNLOCKED;
static board_wifi_coprocessor_info_t s_coprocessor = {
    .chip = CONFIG_ESP_HOSTED_IDF_SLAVE_TARGET,
};

/* The query is an RPC over the hosted link, so it runs once here instead of from UI code. */
static void wifi_coprocessor_version_load(void)
{
    esp_hosted_coprocessor_fwver_t ver = {};
    esp_err_t ret = esp_hosted_get_coprocessor_fwversion(&ver);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Read %s firmware version failed: %s", s_coprocessor.chip, esp_err_to_name(ret));
        return;
    }
    s_coprocessor.major = ver.major1;
    s_coprocessor.minor = ver.minor1;
    s_coprocessor.patch = ver.patch1;
    s_coprocessor.valid = true;
    ESP_LOGI(TAG, "Co-processor %s ESP-Hosted firmware %" PRIu32 ".%" PRIu32 ".%" PRIu32, s_coprocessor.chip, s_coprocessor.major, s_coprocessor.minor, s_coprocessor.patch);
}

static void wifi_link_set(board_wifi_link_state_t state, const uint8_t *ssid, size_t ssid_len)
{
    portENTER_CRITICAL(&s_link_lock);
    s_link.state = state;
    if (ssid != NULL) {
        size_t len = ssid_len < sizeof(s_link.ssid) - 1 ? ssid_len : sizeof(s_link.ssid) - 1;
        memcpy(s_link.ssid, ssid, len);
        s_link.ssid[len] = '\0';
    } else if (state == BOARD_WIFI_LINK_IDLE) {
        s_link.ssid[0] = '\0';
    }
    portEXIT_CRITICAL(&s_link_lock);
}

/* Runs in the publisher task; keeps the link snapshot for pages that open later. */
static void wifi_link_event_handler(const adf_event_t *event, void *ctx)
{
    (void)ctx;

    switch (event->event_id) {
    case ESP_WIFI_SERVICE_EVENT_STA_CONFIG:
        if (event->payload != NULL && event->payload_len >= sizeof(esp_wifi_service_sta_config_event_t)) {
            const esp_wifi_service_sta_config_event_t *info = event->payload;
            if (info->config != NULL) {
                const uint8_t *ssid = info->config->sta.ssid;
                wifi_link_set(BOARD_WIFI_LINK_CONNECTING, ssid, strnlen((const char *)ssid, sizeof(info->config->sta.ssid)));
            }
        }
        break;
    case ESP_WIFI_SERVICE_EVENT_CONNECTED:
        if (event->payload != NULL && event->payload_len >= sizeof(wifi_event_sta_connected_t)) {
            const wifi_event_sta_connected_t *info = event->payload;
            wifi_link_set(BOARD_WIFI_LINK_ASSOCIATED, info->ssid, info->ssid_len);
        }
        break;
    case ESP_WIFI_SERVICE_EVENT_STA_GOT_IP:
        wifi_link_set(BOARD_WIFI_LINK_GOT_IP, NULL, 0);
        break;
    case ESP_WIFI_SERVICE_EVENT_STA_LOST_IP:
        portENTER_CRITICAL(&s_link_lock);
        if (s_link.state == BOARD_WIFI_LINK_GOT_IP) {
            s_link.state = BOARD_WIFI_LINK_ASSOCIATED;
        }
        portEXIT_CRITICAL(&s_link_lock);
        break;
    case ESP_WIFI_SERVICE_EVENT_DISCONNECTED: {
        /* Switching networks first disconnects from the old AP; keep the new attempt in that case. */
        const wifi_event_sta_disconnected_t *info =
            (event->payload != NULL && event->payload_len >= sizeof(wifi_event_sta_disconnected_t)) ? event->payload : NULL;
        portENTER_CRITICAL(&s_link_lock);
        const bool other_ap = info != NULL && s_link.state == BOARD_WIFI_LINK_CONNECTING && (info->ssid_len != strlen(s_link.ssid) || memcmp(info->ssid, s_link.ssid, info->ssid_len) != 0);
        if (!other_ap) {
            s_link.state = BOARD_WIFI_LINK_IDLE;
            s_link.ssid[0] = '\0';
        }
        portEXIT_CRITICAL(&s_link_lock);
        break;
    }
    default:
        break;
    }
}

static bool wifi_enable_load(void)
{
    nvs_handle_t handle;
    int32_t value = 0;
    if (nvs_open(WIFI_ENABLE_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_i32(handle, WIFI_ENABLE_NVS_KEY, &value);
        nvs_close(handle);
    }
    return value != 0;
}

static esp_err_t wifi_enable_save(bool enable)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(WIFI_ENABLE_NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "Open NVS failed");
    esp_err_t ret = nvs_set_i32(handle, WIFI_ENABLE_NVS_KEY, enable ? 1 : 0);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

/* Earlier firmware stored the station credentials through esp_wifi (WIFI_STORAGE_FLASH). Import them
 * once so existing devices keep connecting after the switch to the profile manager. */
static void wifi_import_legacy_config(void)
{
    uint8_t count = 0;
    if (esp_wifi_service_profile_mgr_count(s_profile_mgr, &count) != ESP_OK || count > 0) {
        return;
    }

    wifi_config_t config = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK || config.sta.ssid[0] == '\0') {
        return;
    }

    esp_wifi_service_profile_t profile = {
        .flags = ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED,
        .priority = 0,
    };
    strlcpy(profile.ssid, (const char *)config.sta.ssid, sizeof(profile.ssid));
    strlcpy(profile.password, (const char *)config.sta.password, sizeof(profile.password));
    esp_err_t ret = esp_wifi_service_profile_mgr_add(s_profile_mgr, &profile);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Imported legacy Wi-Fi profile '%s'", profile.ssid);
    } else {
        ESP_LOGW(TAG, "Import legacy Wi-Fi profile failed: %s", esp_err_to_name(ret));
    }
}

static esp_err_t wifi_service_apply_enabled(bool enable)
{
    esp_service_t *service = ESP_SERVICE_BASE(s_wifi_service);
    bool running = false;
    ESP_RETURN_ON_ERROR(esp_service_is_running(service, &running), TAG, "Query service state failed");

    if (enable) {
        if (!running) {
            ESP_RETURN_ON_ERROR(esp_service_start(service), TAG, "Start Wi-Fi service failed");
        }
        return ESP_OK;
    }

    if (running) {
        ESP_RETURN_ON_ERROR(esp_service_stop(service), TAG, "Stop Wi-Fi service failed");
    }
    /* A pending attempt never reports back once the selector is stopped. */
    wifi_link_set(BOARD_WIFI_LINK_IDLE, NULL, 0);
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_CONNECT && ret != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "Disconnect failed: %s", esp_err_to_name(ret));
    }
    return ESP_OK;
}

esp_err_t board_wifi_service_init(void)
{
    if (s_wifi_service != NULL) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    if (s_wifi_lock == NULL) {
        s_wifi_lock = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(s_wifi_lock, ESP_ERR_NO_MEM, TAG, "Create Wi-Fi lock failed");
    }

    ESP_GOTO_ON_ERROR(esp_config_storage_init_nvs(&s_profile_store, &s_profile_storage), err, TAG, "Init profile storage failed");

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = WIFI_PROFILE_MAX_NUM,
        .storage = s_profile_storage,
        .crypto = NULL,
        .crypto_extra_size = 0,
    };
    ESP_GOTO_ON_ERROR(esp_wifi_service_profile_mgr_init(&profile_cfg, &s_profile_mgr), err, TAG, "Init profile manager failed");

    /* Subscribe before the service starts so the first automatic connect attempt is tracked. */
    ESP_GOTO_ON_ERROR(board_wifi_service_subscribe(ADF_EVENT_ANY_ID, wifi_link_event_handler, NULL), err, TAG, "Subscribe Wi-Fi link events failed");

    /* No provisioning agents: credentials are entered from the Settings UI. */
    esp_wifi_service_config_t cfg = {
        .name = WIFI_SERVICE_NAME,
        .profile_manager = s_profile_mgr,
        .prov_list = NULL,
        .prov_num = 0,
        .selector_policy = &s_selector_policy,
    };
    /* Creating the service initializes esp_netif, the event loop and the Wi-Fi driver in STA mode. */
    ESP_GOTO_ON_ERROR(esp_wifi_service_create(&cfg, &s_wifi_service), err, TAG, "Create Wi-Fi service failed");

    // The Wi-Fi driver init above brought up the hosted link to the co-processor.
    // wifi_coprocessor_version_load();

    wifi_import_legacy_config();

    s_wifi_enabled = wifi_enable_load();
    ESP_LOGI(TAG, "Wi-Fi service ready, enabled: %d", s_wifi_enabled);
    if (s_wifi_enabled) {
        ret = wifi_service_apply_enabled(true);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Start Wi-Fi service failed: %s", esp_err_to_name(ret));
        }
    }
    return ESP_OK;

err:
    if (s_wifi_service) {
        esp_wifi_service_destroy(s_wifi_service);
        s_wifi_service = NULL;
    }
    if (s_profile_mgr) {
        esp_wifi_service_profile_mgr_deinit(s_profile_mgr);
        s_profile_mgr = NULL;
    }
    if (s_profile_storage) {
        esp_config_storage_deinit(s_profile_storage);
        s_profile_storage = NULL;
    }
    return ret;
}

bool board_wifi_service_is_ready(void)
{
    return s_wifi_service != NULL;
}

esp_err_t board_wifi_get_coprocessor_info(board_wifi_coprocessor_info_t *info)
{
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "Invalid co-processor info pointer");
    *info = s_coprocessor;
    return ESP_OK;
}

esp_err_t board_wifi_service_subscribe(uint16_t event_id, adf_event_handler_t handler, void *ctx)
{
    ESP_RETURN_ON_FALSE(handler, ESP_ERR_INVALID_ARG, TAG, "Handler is NULL");

    /* Retain the service's event domain so apps can subscribe before the service is created; the
     * service later gets the same hub from adf_event_hub_create() with the same name. */
    if (s_wifi_event_hub == NULL) {
        ESP_RETURN_ON_ERROR(adf_event_hub_create(WIFI_SERVICE_NAME, &s_wifi_event_hub), TAG, "Create Wi-Fi event hub failed");
    }

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = event_id;
    info.handler = handler;
    info.handler_ctx = ctx;
    return adf_event_hub_subscribe(s_wifi_event_hub, &info);
}

esp_wifi_service_scan_handle_t board_wifi_service_get_scan_handle(void)
{
    esp_wifi_service_scan_handle_t handle = NULL;
    if (s_wifi_service) {
        (void)esp_wifi_service_get_scan_handle(s_wifi_service, &handle);
    }
    return handle;
}

esp_err_t board_wifi_service_request_connect(const char *ssid, const char *password, uint8_t priority, uint32_t wait_sec)
{
    ESP_RETURN_ON_FALSE(s_wifi_service, ESP_ERR_INVALID_STATE, TAG, "Wi-Fi service is not initialized");
    ESP_RETURN_ON_FALSE(ssid, ESP_ERR_INVALID_ARG, TAG, "SSID is NULL");

    /* The service API takes mutable buffers; copy so callers can pass string literals. */
    char ssid_buf[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1];
    char password_buf[ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN + 1];
    ESP_RETURN_ON_FALSE(strlcpy(ssid_buf, ssid, sizeof(ssid_buf)) < sizeof(ssid_buf), ESP_ERR_INVALID_ARG, TAG, "SSID too long");
    if (strlcpy(password_buf, password ? password : "", sizeof(password_buf)) >= sizeof(password_buf)) {
        memset(password_buf, 0, sizeof(password_buf));
        ESP_LOGE(TAG, "Password too long");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_wifi_service_request_connect(s_wifi_service, ssid_buf, password_buf, priority, wait_sec);
    memset(password_buf, 0, sizeof(password_buf));
    return ret;
}

esp_err_t board_wifi_service_request_reeval(void)
{
    ESP_RETURN_ON_FALSE(s_wifi_service, ESP_ERR_INVALID_STATE, TAG, "Wi-Fi service is not initialized");
    return esp_wifi_service_request_reeval(s_wifi_service);
}

esp_wifi_service_profile_mgr_t board_wifi_service_get_profile_manager(void)
{
    return s_profile_mgr;
}

esp_err_t board_wifi_service_set_enabled(bool enable)
{
    ESP_RETURN_ON_FALSE(s_wifi_service, ESP_ERR_INVALID_STATE, TAG, "Wi-Fi service is not initialized");

    xSemaphoreTake(s_wifi_lock, portMAX_DELAY);
    esp_err_t ret = wifi_enable_save(enable);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Save Wi-Fi enable state failed: %s", esp_err_to_name(ret));
    }
    s_wifi_enabled = enable;
    ret = wifi_service_apply_enabled(enable);
    xSemaphoreGive(s_wifi_lock);
    return ret;
}

bool board_wifi_service_is_enabled(void)
{
    return s_wifi_enabled;
}

void board_wifi_service_get_link(board_wifi_link_t *link)
{
    if (link == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_link_lock);
    *link = s_link;
    portEXIT_CRITICAL(&s_link_lock);
}

bool board_wifi_service_is_connected(void)
{
    if (s_wifi_service == NULL) {
        /* esp_netif is initialized by the service. */
        return false;
    }
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(WIFI_STA_NETIF_KEY);
    if (netif == NULL) {
        return false;
    }
    esp_netif_ip_info_t info = {0};
    return esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0;
}
