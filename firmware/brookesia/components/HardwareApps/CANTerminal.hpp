#pragma once

#include <atomic>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

class CANTerminal: public systems::phone::App {
public:
    static CANTerminal *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
    ~CANTerminal();

protected:
    CANTerminal(bool use_status_bar, bool use_navigation_bar);
    bool run() override;
    bool back() override;
    bool close() override;
    bool init() override;
    bool deinit() override;
    bool pause() override;
    bool resume() override;

private:
    static constexpr uint32_t AUTO_SEND_PERIOD_MS = 1000;
    static CANTerminal *_instance;

    lv_obj_t *_root = nullptr;
    lv_obj_t *_auto_switch = nullptr;
    lv_obj_t *_status_label = nullptr;
    lv_obj_t *_tx_count_label = nullptr;
    lv_obj_t *_rx_count_label = nullptr;
    lv_obj_t *_last_frame_label = nullptr;
    lv_timer_t *_ui_timer = nullptr;
    SemaphoreHandle_t _frame_mutex = nullptr;
    TaskHandle_t _worker_task = nullptr;
    std::atomic<bool> _worker_running{false};
    std::atomic<bool> _worker_exited{true};
    std::atomic<bool> _auto_send{false};
    std::atomic<uint32_t> _tx_count{0};
    std::atomic<uint32_t> _rx_count{0};
    std::atomic<int> _bus_state{0};
    std::atomic<uint32_t> _tx_errors{0};
    std::atomic<uint32_t> _rx_errors{0};
    std::atomic<uint32_t> _bus_errors{0};
    char _last_frame[96] = "Waiting for CAN frames...";
    esp_err_t _init_result = ESP_ERR_INVALID_STATE;

    void createUi();
    void destroyUi();
    bool startWorker();
    void stopWorker();
    void worker();
    bool sendTestFrame();
    void setLastFrame(const char *text);
    void showReceivedFrame(uint32_t identifier, const uint8_t *data, uint8_t length, bool extended);
    void refreshUi();
    static void workerEntry(void *arg);
    static void uiTimerCallback(lv_timer_t *timer);
    static void autoSwitchCallback(lv_event_t *event);
    static void sendButtonCallback(lv_event_t *event);
};

} // namespace esp_brookesia::apps
