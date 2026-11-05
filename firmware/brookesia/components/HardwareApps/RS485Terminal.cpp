/* SPDX-FileCopyrightText: 2026 Waveshare */
/* SPDX-License-Identifier: Apache-2.0 */

#include "RS485Terminal.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "HardwareUI.hpp"
#include "board_peripherals.h"
#include "esp_lib_utils.h"

LV_IMG_DECLARE(img_app_rs485);

namespace esp_brookesia::apps {
namespace {
constexpr char AUTO_MESSAGE[] = "Hi,this is Waveshare ESP32-P4-WIFI6-Touch-LCD-7B!\r\n";
void setLabelIfChanged(lv_obj_t *label, const char *text)
{
    if (label && strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}
} // namespace

RS485Terminal *RS485Terminal::_instance = nullptr;
RS485Terminal *RS485Terminal::requestInstance(bool status, bool navigation)
{
    if (!_instance)
        _instance = new RS485Terminal(status, navigation);
    return _instance;
}
RS485Terminal::RS485Terminal(bool status, bool navigation) : App("RS485", &img_app_rs485, true, status, navigation)
{
}
RS485Terminal::~RS485Terminal()
{
    deinit();
}

bool RS485Terminal::init()
{
    if (!_rx_mutex)
        _rx_mutex = xSemaphoreCreateMutex();
    return _rx_mutex != nullptr;
}
bool RS485Terminal::run()
{
    if (_init_result != ESP_OK)
        _init_result = board_rs485_init();
    createUi();
    if (_init_result == ESP_OK && !startWorker()) {
        (void)board_rs485_deinit();
        _init_result = ESP_ERR_NO_MEM;
        setLabelIfChanged(_rx_text_label, "RS485 worker task creation failed");
    }
    return true;
}
bool RS485Terminal::back()
{
    stopWorker();
    return notifyCoreClosed();
}
bool RS485Terminal::close()
{
    stopWorker();
    if (_worker_task)
        return false;
    destroyUi();
    esp_err_t ret = board_rs485_deinit();
    _init_result = ESP_ERR_INVALID_STATE;
    return ret == ESP_OK;
}
bool RS485Terminal::deinit()
{
    stopWorker();
    if (_worker_task)
        return false;
    destroyUi();
    esp_err_t ret = board_rs485_deinit();
    _init_result = ESP_ERR_INVALID_STATE;
    if (_rx_mutex) {
        vSemaphoreDelete(_rx_mutex);
        _rx_mutex = nullptr;
    }
    return ret == ESP_OK;
}
bool RS485Terminal::pause()
{
    stopWorker();
    return _worker_task == nullptr;
}
bool RS485Terminal::resume()
{
    return startWorker();
}

void RS485Terminal::createUi()
{
    destroyUi();
    lv_obj_t *content = nullptr;
    _root = hardware_ui::create_page("RS485 Terminal", &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 14, LV_PART_MAIN);
    lv_obj_t *tx_panel = hardware_ui::create_panel(content);
    lv_obj_set_size(tx_panel, lv_pct(100), 188);
    lv_obj_set_style_pad_all(tx_panel, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(tx_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tx_panel, 10, LV_PART_MAIN);
    lv_obj_t *header = lv_obj_create(tx_panel);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, lv_pct(100), 42);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Transmit / 115200 8N1");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, LV_PART_MAIN);
    _auto_switch = lv_switch_create(header);
    lv_obj_set_size(_auto_switch, 82, 42);
    if (_auto_send.load())
        lv_obj_add_state(_auto_switch, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(_auto_switch, lv_color_hex(hardware_ui::COLOR_SUCCESS), static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_add_event_cb(_auto_switch, autoSwitchCallback, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_t *message = lv_label_create(tx_panel);
    lv_obj_set_width(message, lv_pct(100));
    lv_label_set_long_mode(message, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(message, "Automatic direction; no RTS/DE control. Sends a 7B greeting every second while enabled.");
    hardware_ui::style_secondary_label(message, &lv_font_montserrat_18);
    _tx_count_label = lv_label_create(tx_panel);
    hardware_ui::style_secondary_label(_tx_count_label);
    lv_obj_t *rx_panel = hardware_ui::create_panel(content);
    lv_obj_set_width(rx_panel, lv_pct(100));
    lv_obj_set_flex_grow(rx_panel, 1);
    lv_obj_set_style_pad_all(rx_panel, 18, LV_PART_MAIN);
    lv_obj_set_flex_flow(rx_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *rx_header = lv_obj_create(rx_panel);
    lv_obj_remove_style_all(rx_header);
    lv_obj_set_size(rx_header, lv_pct(100), 48);
    lv_obj_set_flex_flow(rx_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rx_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _rx_count_label = lv_label_create(rx_header);
    lv_obj_set_style_text_font(_rx_count_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_t *clear = hardware_ui::create_icon_button(rx_header, LV_SYMBOL_TRASH);
    lv_obj_add_event_cb(clear, clearButtonCallback, LV_EVENT_CLICKED, this);
    lv_obj_t *scroll = lv_obj_create(rx_panel);
    lv_obj_remove_style_all(scroll);
    lv_obj_set_width(scroll, lv_pct(100));
    lv_obj_set_flex_grow(scroll, 1);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_style_bg_color(scroll, lv_color_hex(0x101012), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(scroll, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scroll, 14, LV_PART_MAIN);
    _rx_text_label = lv_label_create(scroll);
    lv_obj_set_width(_rx_text_label, lv_pct(100));
    lv_label_set_long_mode(_rx_text_label, LV_LABEL_LONG_MODE_WRAP);
    hardware_ui::style_secondary_label(_rx_text_label);
    lv_label_set_text(_rx_text_label, _init_result == ESP_OK ? "Waiting for data..." : "RS485 initialization failed");
    _ui_timer = lv_timer_create(uiTimerCallback, 200, this);
    refreshUi();
}
void RS485Terminal::destroyUi()
{
    if (_ui_timer) {
        lv_timer_delete(_ui_timer);
        _ui_timer = nullptr;
    }
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    _auto_switch = _tx_count_label = _rx_count_label = _rx_text_label = nullptr;
}
bool RS485Terminal::startWorker()
{
    if (_worker_task && _worker_exited.load())
        _worker_task = nullptr;
    if (_worker_task)
        return false;
    if (_init_result != ESP_OK)
        return false;
    _worker_exited.store(false);
    _worker_running.store(true);
    if (xTaskCreatePinnedToCore(workerEntry, "rs485_terminal", 4096, this, 5, &_worker_task, 0) != pdPASS) {
        _worker_running.store(false);
        _worker_exited.store(true);
        _worker_task = nullptr;
        return false;
    }
    return true;
}
void RS485Terminal::stopWorker()
{
    if (!_worker_task)
        return;
    _worker_running.store(false);
    if (xTaskGetCurrentTaskHandle() == _worker_task)
        return;
    TickType_t start = xTaskGetTickCount();
    while (!_worker_exited.load() && xTaskGetTickCount() - start < pdMS_TO_TICKS(1000)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (_worker_exited.load())
        _worker_task = nullptr;
}
void RS485Terminal::worker()
{
    TickType_t next_send = xTaskGetTickCount();
    uint8_t buffer[128];
    while (_worker_running.load()) {
        TickType_t now = xTaskGetTickCount();
        if (_auto_send.load() && (int32_t)(now - next_send) >= 0) {
            size_t written = 0;
            if (board_rs485_write(AUTO_MESSAGE, sizeof(AUTO_MESSAGE) - 1, &written) == ESP_OK && written == sizeof(AUTO_MESSAGE) - 1)
                _tx_count.fetch_add(1);
            next_send = now + pdMS_TO_TICKS(AUTO_SEND_PERIOD_MS);
        }
        size_t read = 0;
        if (board_rs485_read(buffer, sizeof(buffer), &read, 20) == ESP_OK && read) {
            appendReceived(buffer, read);
            _rx_count.fetch_add(read);
        }
    }
    _worker_exited.store(true);
    vTaskDelete(nullptr);
}
void RS485Terminal::appendReceived(const uint8_t *data, size_t size)
{
    if (!_rx_mutex || xSemaphoreTake(_rx_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return;
    for (size_t i = 0; i < size; ++i) {
        char encoded[5] = {};
        size_t length = 0;
        if (data[i] == '\r' || data[i] == '\n' || data[i] == '\t' || isprint(data[i])) {
            encoded[0] = (char)data[i];
            length = 1;
        } else {
            snprintf(encoded, sizeof(encoded), "\\x%02X", data[i]);
            length = 4;
        }
        if (_rx_text_length + length + 1 >= RX_TEXT_CAPACITY) {
            const size_t keep = RX_TEXT_CAPACITY / 2;
            memmove(_rx_text, _rx_text + _rx_text_length - keep, keep);
            _rx_text_length = keep;
        }
        memcpy(_rx_text + _rx_text_length, encoded, length);
        _rx_text_length += length;
        _rx_text[_rx_text_length] = '\0';
    }
    xSemaphoreGive(_rx_mutex);
}
void RS485Terminal::refreshUi()
{
    char text[48];
    snprintf(text, sizeof(text), "TX frames: %lu", (unsigned long)_tx_count.load());
    setLabelIfChanged(_tx_count_label, text);
    snprintf(text, sizeof(text), "Receive / %lu bytes", (unsigned long)_rx_count.load());
    setLabelIfChanged(_rx_count_label, text);
    if (_rx_text_label && _rx_mutex && xSemaphoreTake(_rx_mutex, 0) == pdTRUE) {
        if (_rx_text_length)
            setLabelIfChanged(_rx_text_label, _rx_text);
        xSemaphoreGive(_rx_mutex);
    }
}
void RS485Terminal::clearReceived()
{
    if (!_rx_mutex || xSemaphoreTake(_rx_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return;
    _rx_text[0] = '\0';
    _rx_text_length = 0;
    _rx_count.store(0);
    xSemaphoreGive(_rx_mutex);
    if (_rx_text_label)
        lv_label_set_text(_rx_text_label, "Waiting for data...");
}
void RS485Terminal::workerEntry(void *arg)
{
    static_cast<RS485Terminal *>(arg)->worker();
}
void RS485Terminal::uiTimerCallback(lv_timer_t *timer)
{
    auto *self = static_cast<RS485Terminal *>(lv_timer_get_user_data(timer));
    if (self)
        self->refreshUi();
}
void RS485Terminal::autoSwitchCallback(lv_event_t *event)
{
    auto *self = static_cast<RS485Terminal *>(lv_event_get_user_data(event));
    if (self)
        self->_auto_send.store(lv_obj_has_state((lv_obj_t *)lv_event_get_target(event), LV_STATE_CHECKED));
}
void RS485Terminal::clearButtonCallback(lv_event_t *event)
{
    auto *self = static_cast<RS485Terminal *>(lv_event_get_user_data(event));
    if (self)
        self->clearReceived();
}

// The app is a singleton owned by itself; the registry must not delete it.
ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, RS485Terminal, "RS485", []()
{
    return std::shared_ptr<RS485Terminal>(RS485Terminal::requestInstance(), [](RS485Terminal *) {});
})

} // namespace esp_brookesia::apps
