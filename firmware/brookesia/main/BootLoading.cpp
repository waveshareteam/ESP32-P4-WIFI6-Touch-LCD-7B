/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "BootLoading.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BootLoading"
#include "esp_lib_utils.h"

#include "board.h"

using namespace esp_brookesia::gui;

namespace {

constexpr uint32_t BOOT_BACKLIGHT_FADE_MS = 300;
constexpr uint32_t BOOT_BACKLIGHT_FADE_STEP_MS = 15;
constexpr uint32_t BOOT_STAGE_POLL_MS = 15;

void on_boot_frame_rendered(lv_event_t *event)
{
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(lv_event_get_user_data(event)));
}

} // namespace

bool BootLoadingUi::create()
{
    root_ = lv_obj_create(lv_layer_top());
    if (root_ == nullptr) {
        return false;
    }

    spinner_ = lv_spinner_create(root_);
    title_ = lv_label_create(root_);
    phase_ = lv_label_create(root_);
    status_ = lv_label_create(root_);
    progress_ = lv_bar_create(root_);
    if (spinner_ == nullptr || title_ == nullptr || phase_ == nullptr || status_ == nullptr || progress_ == nullptr) {
        destroy();
        return false;
    }

    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x101417), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_size(spinner_, 64, 64);
    lv_obj_align(spinner_, LV_ALIGN_CENTER, 0, -110);
    lv_obj_set_style_arc_width(spinner_, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner_, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner_, lv_color_hex(0x313B42), LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner_, lv_color_hex(0x27C1A8), LV_PART_INDICATOR);

    lv_label_set_text(title_, "ESP32-P4");
    lv_obj_set_style_text_font(title_, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_, lv_color_hex(0xF7F9FA), LV_PART_MAIN);
    lv_obj_align(title_, LV_ALIGN_CENTER, 0, -36);

    lv_label_set_text(phase_, "LOADING");
    lv_obj_set_style_text_font(phase_, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(phase_, lv_color_hex(0xF2B84B), LV_PART_MAIN);
    lv_obj_align(phase_, LV_ALIGN_CENTER, 0, 4);

    lv_obj_set_width(status_, lv_pct(84));
    lv_label_set_long_mode(status_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(status_, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(status_, lv_color_hex(0xA8B1B8), LV_PART_MAIN);
    lv_obj_align(status_, LV_ALIGN_CENTER, 0, 44);

    lv_obj_set_size(progress_, lv_pct(52), 7);
    lv_obj_align(progress_, LV_ALIGN_CENTER, 0, 82);
    lv_bar_set_range(progress_, 0, 100);
    lv_obj_set_style_radius(progress_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(progress_, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_, lv_color_hex(0x2A343B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_, lv_color_hex(0x27C1A8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_INDICATOR);

    setStage("Preparing display...", 8);

    display_ = lv_obj_get_display(root_);
    stage_rendered_ = xSemaphoreCreateBinary();
    stage_timer_ = lv_timer_create(onStageTimer, BOOT_STAGE_POLL_MS, this);
    if (stage_rendered_ == nullptr || stage_timer_ == nullptr) {
        destroy();
        return false;
    }
    lv_display_add_event_cb(display_, onFrameRendered, LV_EVENT_REFR_READY, this);
    return true;
}

bool BootLoadingUi::showStage(const char *status, int progress, uint32_t timeout_ms)
{
    if (stage_rendered_ == nullptr) {
        return false;
    }

    (void)xSemaphoreTake(stage_rendered_, 0);
    pending_progress_.store(progress);
    pending_status_.store(status);
    return xSemaphoreTake(stage_rendered_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void BootLoadingUi::stopAnimation()
{
    stop_animation_.store(true);
}

void BootLoadingUi::onStageTimer(lv_timer_t *timer)
{
    auto *self = static_cast<BootLoadingUi *>(lv_timer_get_user_data(timer));

    if (self->stop_animation_.exchange(false) && self->spinner_ != nullptr) {
        lv_anim_delete(self->spinner_, nullptr);
        lv_obj_add_flag(self->spinner_, LV_OBJ_FLAG_HIDDEN);
    }

    const char *status = self->pending_status_.exchange(nullptr);
    if (status != nullptr) {
        self->setStage(status, self->pending_progress_.load());
        self->stage_applied_ = true;
    }
}

void BootLoadingUi::onFrameRendered(lv_event_t *event)
{
    auto *self = static_cast<BootLoadingUi *>(lv_event_get_user_data(event));

    if (self->stage_applied_) {
        self->stage_applied_ = false;
        xSemaphoreGive(self->stage_rendered_);
    }
}

// app_main owns this object and may return right after, so nothing may keep pointing at it.
void BootLoadingUi::stopStageUpdates()
{
    if (stage_timer_ != nullptr) {
        lv_timer_delete(stage_timer_);
        stage_timer_ = nullptr;
    }
    if (display_ != nullptr) {
        lv_display_remove_event_cb_with_user_data(display_, onFrameRendered, this);
        display_ = nullptr;
    }
    if (stage_rendered_ != nullptr) {
        vSemaphoreDelete(stage_rendered_);
        stage_rendered_ = nullptr;
    }
}

void BootLoadingUi::setStage(const char *status, int progress)
{
    if (root_ == nullptr || status == nullptr) {
        return;
    }

    if (progress < 0) {
        progress = 0;
    } else if (progress > 100) {
        progress = 100;
    }

    lv_label_set_text(status_, status);
    lv_bar_set_value(progress_, progress, LV_ANIM_OFF);
    lv_obj_move_foreground(root_);
}

void BootLoadingUi::showError(const char *status)
{
    if (root_ == nullptr) {
        return;
    }

    stopStageUpdates();
    lv_anim_delete(spinner_, nullptr);
    lv_obj_add_flag(spinner_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(phase_, "STARTUP FAILED");
    lv_obj_set_style_text_color(phase_, lv_color_hex(0xF06A6A), LV_PART_MAIN);
    lv_obj_align(phase_, LV_ALIGN_CENTER, 0, 4);
    lv_label_set_text(status_, status);
    lv_obj_set_style_text_color(status_, lv_color_hex(0xF7F9FA), LV_PART_MAIN);
    lv_bar_set_value(progress_, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_, lv_color_hex(0xF06A6A), LV_PART_INDICATOR);
    lv_obj_move_foreground(root_);
}

void BootLoadingUi::finish()
{
    if (root_ == nullptr) {
        return;
    }

    stopStageUpdates();
    setStage("Ready", 100);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, root_);
    lv_anim_set_values(&fade, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&fade, FADE_OUT_TIME_MS);
    lv_anim_set_delay(&fade, READY_HOLD_MS);
    lv_anim_set_exec_cb(&fade, [](void *object, int32_t opacity) {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(object), opacity, LV_PART_MAIN);
    });
    lv_anim_set_completed_cb(&fade, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&fade);

    // The animation owns root_ now and deletes it from the LVGL task.
    clearObjectPointers();
}

void BootLoadingUi::attachToTopLayer(lv_display_t *display)
{
    attachToLayer(display != nullptr ? lv_display_get_layer_top(display) : nullptr);
}

void BootLoadingUi::attachToSystemLayer(lv_display_t *display)
{
    attachToLayer(display != nullptr ? lv_display_get_layer_sys(display) : nullptr);
}

void BootLoadingUi::destroy()
{
    stopStageUpdates();
    if (root_ != nullptr) {
        lv_obj_delete(root_);
    }
    clearObjectPointers();
}

void BootLoadingUi::clearObjectPointers()
{
    root_ = nullptr;
    spinner_ = nullptr;
    title_ = nullptr;
    phase_ = nullptr;
    status_ = nullptr;
    progress_ = nullptr;
}

void BootLoadingUi::attachToLayer(lv_obj_t *parent)
{
    if (root_ == nullptr || parent == nullptr) {
        return;
    }

    if (lv_obj_get_parent(root_) != parent) {
        lv_obj_set_parent(root_, parent);
        lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
        lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
    }
    lv_obj_move_foreground(root_);
}

/*
 * esp_lv_adapter_refresh_now() must not be used here: with partial tear
 * avoidance its flush waits for a buffer-free notification that the panel ISR
 * sends to the LVGL task, so calling it from app_main deadlocks once all frame
 * buffers are in flight.
 */
bool apply_and_wait_for_frame(lv_display_t *display, const std::function<void()> &change, uint32_t timeout_ms)
{
    static SemaphoreHandle_t frame_rendered = xSemaphoreCreateBinary();

    {
        LvLockGuard gui_guard;
        change();
        if (display == nullptr || frame_rendered == nullptr) {
            return false;
        }
        (void)xSemaphoreTake(frame_rendered, 0);
        // Registered under the same lock as the change, so the next refresh includes it.
        lv_display_add_event_cb(display, on_boot_frame_rendered, LV_EVENT_REFR_READY, frame_rendered);
    }

    const bool rendered = xSemaphoreTake(frame_rendered, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    if (!rendered) {
        ESP_LOGW(ESP_UTILS_LOG_TAG, "Wait for boot loading frame timed out (%lu ms)", static_cast<unsigned long>(timeout_ms));
    }

    LvLockGuard gui_guard;
    lv_display_remove_event_cb_with_user_data(display, on_boot_frame_rendered, frame_rendered);
    return rendered;
}

void refresh_boot_loading(BootLoadingUi &loading, const char *status, int progress)
{
    ESP_LOGI(ESP_UTILS_LOG_TAG, "Startup stage: %s (%d%%)", status, progress);
    if (!loading.showStage(status, progress, BOOT_FRAME_WAIT_MS)) {
        ESP_LOGW(ESP_UTILS_LOG_TAG, "Wait for boot stage frame timed out (%lu ms)", static_cast<unsigned long>(BOOT_FRAME_WAIT_MS));
    }
}

/*
 * Quadratic easing compensates for the eye's non-linear response to
 * brightness, keeping the ramp visually even.
 */
void fade_in_backlight(int target_percent)
{
    const uint32_t steps = BOOT_BACKLIGHT_FADE_MS / BOOT_BACKLIGHT_FADE_STEP_MS;
    for (uint32_t step = 1; step < steps; ++step) {
        const int percent = static_cast<int>(target_percent * step * step / (steps * steps));
        if (board_display_brightness_set(percent) != ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(BOOT_BACKLIGHT_FADE_STEP_MS));
    }
    // Always end exactly at the target, even if a step above failed.
    (void)board_display_brightness_set(target_percent);
}

BootLoadingFailureGuard::~BootLoadingFailureGuard()
{
    if (!armed_) {
        return;
    }

    // A running spinner would keep the LVGL lock busy and delay the error screen.
    loading_.stopAnimation();
    {
        LvLockGuard gui_guard;
        loading_.attachToSystemLayer(display_);
        loading_.showError("Unable to finish startup. Please restart the device.");
    }
    // Startup can fail before the backlight is faded in; make the error visible.
    if (board_display_brightness_get() == 0) {
        (void)board_display_backlight_on();
    }
}

void PhoneStartupOwner::reset()
{
    if (phone_ == nullptr) {
        return;
    }

    if (!board_display_lock(-1)) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Lock display while cleaning up phone failed");
        return;
    }
    loading_.attachToTopLayer(display_);
    delete phone_;
    phone_ = nullptr;
    board_display_unlock();
}
