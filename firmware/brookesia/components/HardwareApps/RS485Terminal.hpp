#pragma once

#include <atomic>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

class RS485Terminal: public systems::phone::App {
public:
    static RS485Terminal *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
    ~RS485Terminal();

protected:
    RS485Terminal(bool use_status_bar, bool use_navigation_bar);
    bool run() override;
    bool back() override;
    bool close() override;
    bool init() override;
    bool deinit() override;
    bool pause() override;
    bool resume() override;

private:
    static constexpr size_t RX_TEXT_CAPACITY = 4096;
    static constexpr uint32_t AUTO_SEND_PERIOD_MS = 1000;
    static RS485Terminal *_instance;

    lv_obj_t *_root = nullptr;
    lv_obj_t *_auto_switch = nullptr;
    lv_obj_t *_tx_count_label = nullptr;
    lv_obj_t *_rx_count_label = nullptr;
    lv_obj_t *_rx_text_label = nullptr;
    lv_timer_t *_ui_timer = nullptr;
    SemaphoreHandle_t _rx_mutex = nullptr;
    TaskHandle_t _worker_task = nullptr;
    std::atomic<bool> _worker_running{false};
    std::atomic<bool> _worker_exited{true};
    std::atomic<bool> _auto_send{true};
    std::atomic<uint32_t> _tx_count{0};
    std::atomic<uint32_t> _rx_count{0};
    char _rx_text[RX_TEXT_CAPACITY] = {};
    size_t _rx_text_length = 0;
    esp_err_t _init_result = ESP_ERR_INVALID_STATE;

    void createUi();
    void destroyUi();
    bool startWorker();
    void stopWorker();
    void worker();
    void appendReceived(const uint8_t *data, size_t size);
    void refreshUi();
    void clearReceived();
    static void workerEntry(void *arg);
    static void uiTimerCallback(lv_timer_t *timer);
    static void autoSwitchCallback(lv_event_t *event);
    static void clearButtonCallback(lv_event_t *event);
};

} // namespace esp_brookesia::apps
