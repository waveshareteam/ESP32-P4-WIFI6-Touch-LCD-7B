/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Settings.hpp"
#include "ui/SettingsUI.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_sntp.h"
#include <atomic>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Settings"
#include "esp_lib_utils.h"

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TIMEOUT_MS 1000
// Settings page icons.
LV_IMG_DECLARE(WIFI);
LV_IMG_DECLARE(sound);
LV_IMG_DECLARE(info);
LV_IMG_DECLARE(BLE);
LV_IMG_DECLARE(backlights);
LV_IMG_DECLARE(arrow);
LV_IMG_DECLARE(img_app_settings);

namespace esp_brookesia::apps
{
    // Set from both the app task and the Wi-Fi service publisher task.
    static std::atomic<bool> s_time_sync_started{false};
    static bool s_time_synced = false;

    static void time_sync_notification_cb(struct timeval *tv)
    {
        (void)tv;
        setenv("TZ", "CST-8", 1);
        tzset();
        s_time_synced = true;
        ESP_UTILS_LOGI("SNTP time synced, timezone CST-8 applied");
    }

    static void start_time_sync_once()
    {
        if (esp_sntp_enabled() || s_time_sync_started.exchange(true)) {
            return;
        }

        // Start SNTP only after the network stack has obtained an address.
        // This avoids retries while Wi-Fi is still connecting.
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        esp_sntp_init();
        ESP_UTILS_LOGI("SNTP time sync started");
    }

    // Runs in the Wi-Fi service publisher task.
    static void wifi_time_event_handler(const adf_event_t *event, void *ctx)
    {
        (void)ctx;

        if (event->event_id == BOARD_WIFI_EVENT_STA_GOT_IP) {
            start_time_sync_once();
        }
    }

    Settings *Settings::_instance = nullptr;

    Settings *Settings::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new Settings(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    Settings::Settings(bool use_status_bar, bool use_navigation_bar)
        : App("Settings", &img_app_settings, true, use_status_bar, use_navigation_bar),
          page_root(nullptr), list1(nullptr), active_page(ActivePage::None)
    {
    }

    Settings::~Settings()
    {
    }

    void Settings::create_settings_ui()
    {
        destroy_settings_ui();
        lv_obj_clean(lv_scr_act());

        page_root = settings_ui::create_page(lv_scr_act(), getContentTopInset());
        settings_ui::create_header(page_root, "Settings");
        settings_ui::init_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);

        list1 = settings_ui::create_content_list(page_root);
        lv_obj_add_style(list1, &style_list, LV_PART_MAIN);

        settings_ui::add_section(list1, "Wireless", style_list_text);
        add_button_with_arrow(&WIFI, "WLAN");

        settings_ui::add_section(list1, "Media", style_list_text);
        add_button_with_arrow(&sound, "Sound");
        add_button_with_arrow(&backlights, "Display");

        settings_ui::add_section(list1, "More", style_list_text);
        add_button_with_arrow(&info, "About");
    }

    void Settings::destroy_settings_ui()
    {
        if (page_root != nullptr)
        {
            lv_obj_del(page_root);
            page_root = nullptr;
            list1 = nullptr;
            settings_ui::reset_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);
        }
    }

    void Settings::add_button_with_arrow(const void *icon, const char *text)
    {
        lv_obj_t *btn = lv_list_add_button(list1, icon, text);
        lv_obj_add_style(btn, &style_list_btn, LV_PART_MAIN);
        lv_obj_add_style(btn, &style_list_btn_pressed, LV_STATE_PRESSED);
        settings_ui::use_ellipsis_for_button_label(btn);
        lv_obj_add_event_cb(btn, event_handler_cb, LV_EVENT_CLICKED, this);

        lv_obj_t *arrow_img = lv_image_create(btn);
        lv_image_set_src(arrow_img, &arrow);
    }

    void Settings::event_handler_cb(lv_event_t *e)
    {
        Settings *instance = static_cast<Settings *>(lv_event_get_user_data(e));
        if (instance != nullptr)
        {
            instance->event_handler(e);
        }
    }

    void Settings::open_page_async_cb(void *user_data)
    {
        const char *page_name = static_cast<const char *>(user_data);
        Settings *instance = Settings::requestInstance();
        if (instance != nullptr && page_name != nullptr)
        {
            instance->open_page(page_name);
        }
    }

    void Settings::event_handler(lv_event_t *e)
    {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
        }

        lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
        const char *txt = lv_list_get_button_text(list1, obj);
        ESP_UTILS_LOGI("Clicked: %s", txt);

        if (strcmp(txt, "WLAN") == 0)
        {
            lv_async_call(open_page_async_cb, (void *)"WLAN");
        }
        else if (strcmp(txt, "Sound") == 0)
        {
            lv_async_call(open_page_async_cb, (void *)"Sound");
        }
        else if (strcmp(txt, "Display") == 0)
        {
            lv_async_call(open_page_async_cb, (void *)"Display");
        }
        else if (strcmp(txt, "About") == 0)
        {
            lv_async_call(open_page_async_cb, (void *)"About");
        }
    }

    void Settings::open_page(const char *page_name)
    {
        destroy_settings_ui();

        if (strcmp(page_name, "WLAN") == 0)
        {
            active_page = ActivePage::Wlan;
            WlanPage::requestInstance(false, false)->run();
        }
        else if (strcmp(page_name, "Sound") == 0)
        {
            active_page = ActivePage::Sound;
            SoundPage::requestInstance(false, false)->run();
        }
        else if (strcmp(page_name, "Display") == 0)
        {
            active_page = ActivePage::Display;
            DisplayPage::requestInstance(false, false)->run();
        }
        else if (strcmp(page_name, "About") == 0)
        {
            active_page = ActivePage::About;
            AboutPage::requestInstance(false, false)->run();
        }
    }

    void Settings::close_active_page()
    {
        switch (active_page)
        {
        case ActivePage::Wlan:
            WlanPage::requestInstance(false, false)->close();
            break;
        case ActivePage::Sound:
            SoundPage::requestInstance(false, false)->close();
            break;
        case ActivePage::Display:
            DisplayPage::requestInstance(false, false)->close();
            break;
        case ActivePage::About:
            AboutPage::requestInstance(false, false)->close();
            break;
        case ActivePage::None:
        default:
            break;
        }
        active_page = ActivePage::None;
    }

    lv_coord_t Settings::getContentTopInset()
    {
        // The core only shrinks the app screen for an opaque, fixed status bar. The phone
        // stylesheets use a transparent one that floats over the app, so reserve its height here.
        if (getActiveConfig().status_bar_visual_mode != systems::phone::StatusBar::VisualMode::SHOW_FIXED) {
            return 0;
        }
        const auto &status_bar = getSystem()->getDisplay().getData().status_bar.data.main;
        if (status_bar.background_color.opacity == LV_OPA_COVER) {
            return 0;
        }
        return static_cast<lv_coord_t>(status_bar.size.height);
    }

    void Settings::showRootPage()
    {
        close_active_page();
        create_settings_ui();
    }

    int Settings::pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
    {
        Settings *instance = Settings::requestInstance();
        esp_err_t ret = i2c_master_transmit_receive(instance->pmu_dev_handle, &regAddr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
        if (ret != ESP_OK)
        {
            ESP_UTILS_LOGI("PMU READ FAILED!");
            return -1;
        }
        return 0;
    }

    int Settings::pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
    {
        Settings *instance = Settings::requestInstance();
        uint8_t *buffer = (uint8_t *)malloc(len + 1);
        if (!buffer)
            return -1;
        buffer[0] = regAddr;
        memcpy(&buffer[1], data, len);

        esp_err_t ret = i2c_master_transmit(instance->pmu_dev_handle, buffer, len + 1, I2C_MASTER_TIMEOUT_MS);
        free(buffer);

        if (ret != ESP_OK)
        {
            ESP_UTILS_LOGI("PMU WRITE FAILED!");
            return -1;
        }
        return 0;
    }

    bool Settings::run(void)
    {
        ESP_UTILS_LOGD("Run");
        active_page = ActivePage::None;
        if (page_root == nullptr)
        {
            create_settings_ui();
        }
        return true;
    }

    bool Settings::back(void)
    {
        ESP_UTILS_LOGD("Back");
        if (active_page != ActivePage::None)
        {
            showRootPage();
            return true;
        }
        return notifyCoreClosed();
    }

    bool Settings::close(void)
    {
        ESP_UTILS_LOGD("Close");
        close_active_page();
        destroy_settings_ui();
        return true;
    }

    bool Settings::init()
    {
        ESP_UTILS_LOGD("Init");
        subscribeWifiEvents();
        return true;
    }

    bool Settings::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool Settings::pause()
    {
        ESP_UTILS_LOGD("Pause");
        return true;
    }

    bool Settings::resume()
    {
        ESP_UTILS_LOGD("Resume");
        return true;
    }

    esp_err_t Settings::subscribeWifiEvents()
    {
        static bool subscribed = false;
        if (subscribed) {
            return ESP_OK;
        }

        esp_err_t ret = board_wifi_service_subscribe(
            BOARD_WIFI_EVENT_STA_GOT_IP, wifi_time_event_handler, nullptr);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("Subscribe Wi-Fi service events failed: %s", esp_err_to_name(ret));
            return ret;
        }
        subscribed = true;

        // Covers Settings being subscribed after the station already obtained an address.
        if (board_wifi_service_is_connected()) {
            start_time_sync_once();
        }
        return ESP_OK;
    }

    // The app is a singleton owned by itself; the registry must not delete it.
    // Keep the status bar visible: the core then shrinks the app screen below it.
    ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, Settings, "Settings", []()
    {
        return std::shared_ptr<Settings>(
                   Settings::requestInstance(/* use_status_bar */ true, /* use_navigation_bar */ false),
                   [](Settings *) {});
    })

} // namespace esp_brookesia::apps
