#include "AboutPage.hpp"
#include "SettingsUI.hpp"
#include "esp_app_desc.h"
#include "esp_board_manager.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "sdkconfig.h"
#include "../Settings.hpp"

#define ESP_UTILS_LOG_TAG "BS:AboutPage"
#include "esp_lib_utils.h"

LV_IMG_DECLARE(logo);

namespace {

// Board identity comes from board_info.yaml through esp_board_manager.
const esp_board_info_t &board_info()
{
    static esp_board_info_t info = {};
    static bool loaded = false;
    if (!loaded) {
        loaded = esp_board_manager_get_board_info(&info) == ESP_OK;
    }
    return info;
}

const char *or_unknown(const char *value)
{
    return (value != nullptr && value[0] != '\0') ? value : "Unknown";
}

const char *product_name()
{
    // The description is "<manufacturer> <product>"; the manufacturer has its own row.
    const esp_board_info_t &info = board_info();
    const char *description = info.description;
    if (description != nullptr && info.manufacturer != nullptr) {
        const size_t len = std::strlen(info.manufacturer);
        if (len > 0 && std::strncmp(description, info.manufacturer, len) == 0 && description[len] == ' ') {
            description += len + 1;
        }
    }
    return or_unknown(description);
}

// "esp32p4" -> "ESP32-P4"
void format_chip(const char *chip, char *out, size_t out_size)
{
    chip = or_unknown(chip);
    const char *suffix = std::strncmp(chip, "esp32", 5) == 0 && chip[5] != '\0' ? chip + 5 : nullptr;
    char upper[16] = {};
    const char *src = suffix ? suffix : chip;
    for (size_t i = 0; src[i] != '\0' && i < sizeof(upper) - 1; ++i) {
        upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(src[i])));
    }
    std::snprintf(out, out_size, "%s%s", suffix ? "ESP32-" : "", upper);
}

const char *chip_name()
{
    static char name[40] = {};
    if (name[0] != '\0') {
        return name;
    }

    // Add the silicon revision the firmware actually runs on.
    char chip[24] = {};
    format_chip(board_info().chip, chip, sizeof(chip));
    esp_chip_info_t info = {};
    esp_chip_info(&info);
    std::snprintf(name, sizeof(name), "%s v%d.%d", chip, info.revision / 100, info.revision % 100);
    return name;
}

const char *coprocessor_name()
{
    static char name[24] = {};
    board_wifi_coprocessor_info_t info = {};
    board_wifi_get_coprocessor_info(&info);
    format_chip(info.chip, name, sizeof(name));
    return name;
}

const char *coprocessor_firmware()
{
    // Read once at boot over the hosted link; Unknown when Wi-Fi never came up.
    static char version[24] = {};
    board_wifi_coprocessor_info_t info = {};
    board_wifi_get_coprocessor_info(&info);
    if (!info.valid) {
        return "Unknown";
    }
    std::snprintf(version, sizeof(version), "%lu.%lu.%lu", static_cast<unsigned long>(info.major),
                  static_cast<unsigned long>(info.minor), static_cast<unsigned long>(info.patch));
    return version;
}

const char *display_resolution()
{
    static char resolution[16] = {};
    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        return "Unknown";
    }
    std::snprintf(resolution, sizeof(resolution), "%ld x %ld",
                  static_cast<long>(lv_display_get_horizontal_resolution(disp)),
                  static_cast<long>(lv_display_get_vertical_resolution(disp)));
    return resolution;
}

const char *firmware_version()
{
    static char version[7] = {};
    if (version[0] != '\0') {
        return version;
    }

    // Use the firmware build date, including when this file is not recompiled.
    const char *date = esp_app_get_description()->date;
    char month_name[4] = {};
    int day = 0;
    int year = 0;
    if (std::sscanf(date, "%3s %d %d", month_name, &day, &year) != 3 ||
        day < 1 || day > 31 || year < 0) {
        return "Unknown";
    }
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    for (int month = 0; month < 12; ++month) {
        if (std::strcmp(month_name, months[month]) == 0) {
            std::snprintf(version, sizeof(version), "%02d%02d%02d", year % 100, month + 1, day);
            return version;
        }
    }
    return "Unknown";
}

} // namespace

namespace esp_brookesia::apps
{
    AboutPage *AboutPage::_instance = nullptr;

    AboutPage *AboutPage::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new AboutPage(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    AboutPage::AboutPage(bool use_status_bar, bool use_navigation_bar)
        : App("About", nullptr, true, use_status_bar, use_navigation_bar),
          page_root(nullptr), label(nullptr), list1(nullptr)
    {
    }

    AboutPage::~AboutPage()
    {
    }

    bool AboutPage::run()
    {
        ESP_UTILS_LOGD("AboutPage Run");
        lv_obj_clean(lv_scr_act());

        page_root = settings_ui::create_page(lv_scr_act(), Settings::requestInstance()->getContentTopInset());
        settings_ui::create_header(page_root, "About", [](lv_event_t *e) {
            (void)e;
            lv_async_call([](void *param) {
                (void)param;
                Settings::requestInstance()->showRootPage();
            }, nullptr);
        });

        settings_ui::init_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);
        list1 = settings_ui::create_content_list(page_root);
        lv_obj_add_style(list1, &style_list, LV_PART_MAIN);

        settings_ui::add_section(list1, "Device", style_list_text);
        settings_ui::add_info_row(list1, &logo, "Manufacturer", or_unknown(board_info().manufacturer), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "Product", product_name(), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "Board Version", or_unknown(board_info().version), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "Chip", chip_name(), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "Wi-Fi Chip", coprocessor_name(), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "Display", display_resolution(), style_list_btn);

        settings_ui::add_section(list1, "Software", style_list_text);
        settings_ui::add_info_row(list1, nullptr, "Version", firmware_version(), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "ESP-IDF", esp_get_idf_version(), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "Wi-Fi Firmware", coprocessor_firmware(), style_list_btn);
        settings_ui::add_info_row(list1, nullptr, "UI Framework", "ESP-Brookesia", style_list_btn);
        return true;
    }

    bool AboutPage::back()
    {
        ESP_UTILS_LOGD("AboutPage Back");
        Settings::requestInstance()->showRootPage();
        return true;
    }

    bool AboutPage::close()
    {
        ESP_UTILS_LOGD("AboutPage Close");
        if (page_root != nullptr)
        {
            lv_obj_del(page_root);
            page_root = nullptr;
            label = nullptr;
            list1 = nullptr;
            settings_ui::reset_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);
        }
        return true;
    }

} // namespace esp_brookesia::apps
