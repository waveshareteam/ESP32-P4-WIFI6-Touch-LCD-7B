/* SPDX-FileCopyrightText: 2026 Waveshare */
/* SPDX-License-Identifier: Apache-2.0 */

#include "CANTerminal.hpp"

#include <stdio.h>
#include <string.h>

#include "HardwareUI.hpp"
#include "board_peripherals.h"
#include "esp_lib_utils.h"

LV_IMG_DECLARE(img_app_can);

namespace esp_brookesia::apps {
namespace {
constexpr uint32_t TEST_IDENTIFIER = 0x07B;
constexpr uint8_t TEST_DATA[] = {0x57, 0x53, 0x37, 0x42, 0x43, 0x41, 0x4E, 0x01};

void setLabelIfChanged(lv_obj_t *label, const char *text)
{
    if (label && strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

const char *stateName(int state)
{
    switch ((twai_state_t)state) {
    case TWAI_STATE_RUNNING:
        return "RUNNING";
    case TWAI_STATE_BUS_OFF:
        return "BUS OFF";
    case TWAI_STATE_RECOVERING:
        return "RECOVERING";
    case TWAI_STATE_STOPPED:
        return "STOPPED";
    default:
        return "UNAVAILABLE";
    }
}
} // namespace

CANTerminal *CANTerminal::_instance = nullptr;
CANTerminal *CANTerminal::requestInstance(bool status, bool navigation)
{
    if (!_instance)
        _instance = new CANTerminal(status, navigation);
    return _instance;
}
CANTerminal::CANTerminal(bool status, bool navigation) : App("CAN", &img_app_can, true, status, navigation)
{
}
CANTerminal::~CANTerminal()
{
    deinit();
}

bool CANTerminal::init()
{
    if (!_frame_mutex)
        _frame_mutex = xSemaphoreCreateMutex();
    return _frame_mutex != nullptr;
}
bool CANTerminal::run()
{
    if (_init_result != ESP_OK)
        _init_result = board_twai_init();
    createUi();
    if (_init_result == ESP_OK && !startWorker()) {
        (void)board_twai_deinit();
        _init_result = ESP_ERR_NO_MEM;
        setLabelIfChanged(_status_label, "CAN worker task creation failed");
    }
    return true;
}
bool CANTerminal::back()
{
    stopWorker();
    return notifyCoreClosed();
}
bool CANTerminal::close()
{
    stopWorker();
    if (_worker_task)
        return false;
    _auto_send.store(false);
    destroyUi();
    esp_err_t ret = board_twai_deinit();
    _init_result = ESP_ERR_INVALID_STATE;
    return ret == ESP_OK;
}
bool CANTerminal::deinit()
{
    stopWorker();
    if (_worker_task)
        return false;
    _auto_send.store(false);
    destroyUi();
    esp_err_t ret = board_twai_deinit();
    _init_result = ESP_ERR_INVALID_STATE;
    if (_frame_mutex) {
        vSemaphoreDelete(_frame_mutex);
        _frame_mutex = nullptr;
    }
    return ret == ESP_OK;
}
bool CANTerminal::pause()
{
    stopWorker();
    return _worker_task == nullptr;
}
bool CANTerminal::resume()
{
    return startWorker();
}

void CANTerminal::createUi()
{
    destroyUi();
    lv_obj_t *content = nullptr;
    _root = hardware_ui::create_page("CAN / TWAI", &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 12, LV_PART_MAIN);

    lv_obj_t *tx_panel = hardware_ui::create_panel(content);
    lv_obj_set_size(tx_panel, lv_pct(100), 190);
    lv_obj_set_style_pad_all(tx_panel, 18, LV_PART_MAIN);
    lv_obj_set_flex_flow(tx_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tx_panel, 10, LV_PART_MAIN);
    lv_obj_t *header = lv_obj_create(tx_panel);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, lv_pct(100), 42);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "500 kbit/s / normal mode");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, LV_PART_MAIN);
    _auto_switch = lv_switch_create(header);
    lv_obj_set_size(_auto_switch, 82, 42);
    if (_auto_send.load()) {
        lv_obj_add_state(_auto_switch, LV_STATE_CHECKED);
    }
    lv_obj_set_style_bg_color(_auto_switch, lv_color_hex(hardware_ui::COLOR_SUCCESS), static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_add_event_cb(_auto_switch, autoSwitchCallback, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_t *controls = lv_obj_create(tx_panel);
    lv_obj_remove_style_all(controls);
    lv_obj_set_size(controls, lv_pct(100), 54);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *frame = lv_label_create(controls);
    lv_label_set_text(frame, "Frame 0x07B  57 53 37 42 43 41 4E 01");
    hardware_ui::style_secondary_label(frame, &lv_font_montserrat_16);
    lv_obj_t *send = lv_button_create(controls);
    lv_obj_set_size(send, 112, 48);
    lv_obj_set_style_radius(send, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(send, lv_color_hex(hardware_ui::COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_t *send_label = lv_label_create(send);
    lv_label_set_text(send_label, "SEND");
    lv_obj_center(send_label);
    lv_obj_add_event_cb(send, sendButtonCallback, LV_EVENT_CLICKED, this);
    _tx_count_label = lv_label_create(tx_panel);
    hardware_ui::style_secondary_label(_tx_count_label);

    lv_obj_t *rx_panel = hardware_ui::create_panel(content);
    lv_obj_set_width(rx_panel, lv_pct(100));
    lv_obj_set_flex_grow(rx_panel, 1);
    lv_obj_set_style_pad_all(rx_panel, 18, LV_PART_MAIN);
    lv_obj_set_flex_flow(rx_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rx_panel, 10, LV_PART_MAIN);
    _status_label = lv_label_create(rx_panel);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_22, LV_PART_MAIN);
    _rx_count_label = lv_label_create(rx_panel);
    hardware_ui::style_secondary_label(_rx_count_label);
    _last_frame_label = lv_label_create(rx_panel);
    lv_obj_set_width(_last_frame_label, lv_pct(100));
    lv_label_set_long_mode(_last_frame_label, LV_LABEL_LONG_MODE_WRAP);
    hardware_ui::style_secondary_label(_last_frame_label, &lv_font_montserrat_18);
    lv_obj_t *notice = lv_label_create(rx_panel);
    lv_obj_set_width(notice, lv_pct(100));
    lv_label_set_long_mode(notice, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(notice, "Requires a correctly terminated external CAN peer to acknowledge traffic; this app does not use loopback as a hardware check.");
    lv_obj_set_style_text_color(notice, lv_color_hex(hardware_ui::COLOR_WARNING), LV_PART_MAIN);
    lv_obj_set_style_text_font(notice, &lv_font_montserrat_16, LV_PART_MAIN);
    _ui_timer = lv_timer_create(uiTimerCallback, 200, this);
    refreshUi();
}
void CANTerminal::destroyUi()
{
    if (_ui_timer) {
        lv_timer_delete(_ui_timer);
        _ui_timer = nullptr;
    }
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    _auto_switch = _status_label = _tx_count_label = _rx_count_label = _last_frame_label = nullptr;
}
bool CANTerminal::startWorker()
{
    if (_worker_task && _worker_exited.load())
        _worker_task = nullptr;
    if (_worker_task)
        return false;
    if (_init_result != ESP_OK)
        return false;
    _worker_exited.store(false);
    _worker_running.store(true);
    if (xTaskCreatePinnedToCore(workerEntry, "can_terminal", 4096, this, 5, &_worker_task, 0) != pdPASS) {
        _worker_running.store(false);
        _worker_exited.store(true);
        _worker_task = nullptr;
        return false;
    }
    return true;
}
void CANTerminal::stopWorker()
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
bool CANTerminal::sendTestFrame()
{
    if (_init_result != ESP_OK)
        return false;
    const twai_message_t message = {
        .identifier = TEST_IDENTIFIER,
        .data_length_code = sizeof(TEST_DATA),
        .data = {TEST_DATA[0], TEST_DATA[1], TEST_DATA[2], TEST_DATA[3], TEST_DATA[4], TEST_DATA[5], TEST_DATA[6], TEST_DATA[7]},
    };
    if (board_twai_transmit(&message, 20) != ESP_OK)
        return false;
    _tx_count.fetch_add(1);
    return true;
}
void CANTerminal::worker()
{
    TickType_t next_send = xTaskGetTickCount();
    while (_worker_running.load()) {
        TickType_t now = xTaskGetTickCount();
        if (_auto_send.load() && (int32_t)(now - next_send) >= 0) {
            sendTestFrame();
            next_send = now + pdMS_TO_TICKS(AUTO_SEND_PERIOD_MS);
        }
        twai_message_t message = {};
        if (board_twai_receive(&message, 20) == ESP_OK) {
            showReceivedFrame(message.identifier, message.data, message.data_length_code, message.extd);
            _rx_count.fetch_add(1);
        }
        board_twai_status_t status = {};
        if (board_twai_get_status(&status) == ESP_OK) {
            _bus_state.store((int)status.state);
            _tx_errors.store(status.tx_error_counter);
            _rx_errors.store(status.rx_error_counter);
            _bus_errors.store(status.bus_error_count);
            if (status.state == TWAI_STATE_BUS_OFF || status.state == TWAI_STATE_STOPPED)
                board_twai_recover();
        }
    }
    _worker_exited.store(true);
    vTaskDelete(nullptr);
}
void CANTerminal::setLastFrame(const char *text)
{
    if (!_frame_mutex || xSemaphoreTake(_frame_mutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return;
    snprintf(_last_frame, sizeof(_last_frame), "%s", text);
    xSemaphoreGive(_frame_mutex);
}
void CANTerminal::showReceivedFrame(uint32_t identifier, const uint8_t *data, uint8_t length, bool extended)
{
    char text[sizeof(_last_frame)] = {};
    int used = snprintf(text, sizeof(text), "RX %s 0x%03lX:", extended ? "EXT" : "STD", (unsigned long)identifier);
    for (uint8_t i = 0; i < length && i < 8 && used > 0 && (size_t)used < sizeof(text); ++i)
        used += snprintf(text + used, sizeof(text) - (size_t)used, " %02X", data[i]);
    setLastFrame(text);
}
void CANTerminal::refreshUi()
{
    if (_init_result != ESP_OK) {
        setLabelIfChanged(_status_label, "CAN initialization failed");
        return;
    }
    char text[112];
    snprintf(text, sizeof(text), "Bus: %s  TXerr:%lu RXerr:%lu BusErr:%lu", stateName(_bus_state.load()), (unsigned long)_tx_errors.load(), (unsigned long)_rx_errors.load(), (unsigned long)_bus_errors.load());
    setLabelIfChanged(_status_label, text);
    snprintf(text, sizeof(text), "TX queued: %lu   RX frames: %lu", (unsigned long)_tx_count.load(), (unsigned long)_rx_count.load());
    setLabelIfChanged(_tx_count_label, text);
    setLabelIfChanged(_rx_count_label, "Automatic send is optional; SEND queues one test frame.");
    if (_last_frame_label && _frame_mutex && xSemaphoreTake(_frame_mutex, 0) == pdTRUE) {
        setLabelIfChanged(_last_frame_label, _last_frame);
        xSemaphoreGive(_frame_mutex);
    }
}
void CANTerminal::workerEntry(void *arg)
{
    static_cast<CANTerminal *>(arg)->worker();
}
void CANTerminal::uiTimerCallback(lv_timer_t *timer)
{
    auto *self = static_cast<CANTerminal *>(lv_timer_get_user_data(timer));
    if (self)
        self->refreshUi();
}
void CANTerminal::autoSwitchCallback(lv_event_t *event)
{
    auto *self = static_cast<CANTerminal *>(lv_event_get_user_data(event));
    if (self) {
        self->_auto_send.store(lv_obj_has_state((lv_obj_t *)lv_event_get_target(event), LV_STATE_CHECKED));
    }
}
void CANTerminal::sendButtonCallback(lv_event_t *event)
{
    auto *self = static_cast<CANTerminal *>(lv_event_get_user_data(event));
    if (self) {
        self->sendTestFrame();
    }
}

// The app is a singleton owned by itself; the registry must not delete it.
ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, CANTerminal, "CAN", []()
{
    return std::shared_ptr<CANTerminal>(CANTerminal::requestInstance(), [](CANTerminal *) {});
})

} // namespace esp_brookesia::apps
