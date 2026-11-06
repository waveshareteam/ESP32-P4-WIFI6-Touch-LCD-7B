#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include "esp_brookesia.hpp"
#include "lvgl.h"
#include "esp_wifi_types.h"
#include "board_wifi.h"

namespace esp_brookesia::apps {
    class Settings;
}

namespace esp_brookesia::apps {

/**
 * WLAN settings page.
 *
 * Wi-Fi is owned by the board Wi-Fi service (board component). The page only subscribes to the service
 * events, requests scans from the shared scan agent and asks the service to connect. Service
 * callbacks run outside the LVGL task, so they only record what happened; an LVGL timer applies
 * the changes to the UI.
 */
class WlanPage : public systems::phone::App {
public:
    static WlanPage *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    WlanPage(bool use_status_bar, bool use_navigation_bar);
    virtual ~WlanPage();

    bool run() override;
    bool back() override;
    bool close() override;

private:
    enum class UiState {
        Off,
        Scanning,
        Idle,
        Password,
        Connecting,
    };

    // Bits recorded by service callbacks and consumed by the LVGL timer.
    enum PendingEvent : uint32_t {
        PENDING_SCAN_DONE      = 1u << 0,
        PENDING_CONNECTED      = 1u << 1,
        PENDING_DISCONNECTED   = 1u << 2,
        PENDING_GOT_IP         = 1u << 3,
        PENDING_LOST_IP        = 1u << 4,
        PENDING_CONNECT_FAILED = 1u << 5,
    };

    struct ApInfo {
        char ssid[33];
        int8_t rssi;
        wifi_auth_mode_t authmode;
    };

    static constexpr size_t MAX_AP_NUM = 12;
    static constexpr uint32_t UI_POLL_PERIOD_MS = 100;
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
    static constexpr uint8_t CONNECT_PRIORITY = 10;

    static WlanPage *_instance;

    // UI objects, valid while the page is shown.
    lv_obj_t *page_root = nullptr;
    lv_obj_t *list1 = nullptr;
    lv_obj_t *wlan_switch = nullptr;
    lv_obj_t *password_title = nullptr;
    lv_obj_t *ta = nullptr;
    lv_obj_t *kb = nullptr;
    lv_obj_t *dialog = nullptr;
    lv_obj_t *dialog_footer = nullptr;
    lv_obj_t *connected_text = nullptr;
    lv_obj_t *conn_btn = nullptr;
    lv_obj_t *available_text = nullptr;
    lv_obj_t *refresh_btn = nullptr;
    std::vector<lv_obj_t *> wifi_btns;
    lv_timer_t *ui_timer = nullptr;

    // Styles are owned by the page and reset when the page closes.
    lv_style_t style_list;
    lv_style_t style_list_btn;
    lv_style_t style_list_text;
    lv_style_t style_list_btn_pressed;

    // LVGL-task state.
    UiState ui_state = UiState::Off;
    std::vector<ApInfo> ap_list;
    ApInfo selected_ap = {};
    char wifi_pwd[65] = {};
    char connecting_ssid[33] = {};
    // Link state the list was last built with, to rebuild only when it changes.
    board_wifi_link_t rendered_link = {};
    bool ignore_next_disconnect = false;
    bool restore_profile_on_fail = false;
    esp_wifi_service_profile_t previous_profile = {};
    uint32_t connect_start_tick = 0;
    // A scan requested while the station is joining an AP; started once the attempt settles.
    bool scan_deferred = false;

    // Shared with service callbacks.
    bool events_subscribed = false;
    std::atomic<uint32_t> pending_events{0};
    std::mutex shared_mutex;
    std::vector<wifi_ap_record_t> scan_records;
    esp_err_t scan_error = ESP_OK;
    char disconnected_ssid[33] = {};

    void createUi();
    void destroyUi();
    void subscribeEvents();

    void setUiState(UiState state);
    void rebuildList();
    void clearListRows();
    lv_obj_t *addListRow(const char *text);
    void addRowProgress(lv_obj_t *row, const char *text);
    void showPasswordInput(const ApInfo &ap);
    void hidePasswordInput();
    void showMessage(const char *title, const char *text);
    void showForgetConfirm();
    void createDialog(const char *title, const char *text);
    void addDialogButton(const char *text, intptr_t action, bool primary);
    void closeDialog(bool async);

    void requestScan();
    void startConnect(const ApInfo &ap, const char *password);
    void finishConnect(bool success, bool abort_attempt = false);
    void forgetConnectedNetwork();
    void processPendingEvents();
    void applyScanRecords();

    static void wifi_service_event_cb(const adf_event_t *event, void *ctx);
    static void wifi_scan_done_cb(esp_err_t scan_err, uint16_t ap_count, const wifi_ap_record_t *ap_records, void *ctx);
    static void ui_timer_cb(lv_timer_t *timer);
    static void switch_event_cb(lv_event_t *e);
    static void refresh_event_cb(lv_event_t *e);
    static void ap_btn_event_cb(lv_event_t *e);
    static void conn_btn_event_cb(lv_event_t *e);
    static void kb_event_cb(lv_event_t *e);
    static void dialog_btn_event_cb(lv_event_t *e);
};

} // namespace esp_brookesia::apps
