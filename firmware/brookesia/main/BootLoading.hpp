/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

namespace esp_brookesia::systems::phone {
class Phone;
}

constexpr uint32_t BOOT_FRAME_WAIT_MS = 100;
// The first frame redraws the whole 1024x600 screen in software, which can take
// well over BOOT_FRAME_WAIT_MS.
constexpr uint32_t BOOT_FIRST_FRAME_WAIT_MS = 1000;
// REFR_READY only means LVGL finished drawing. The DPI panel switches to the new
// frame buffer at the next vsync, so allow a couple of panel frames to scan out.
constexpr uint32_t BOOT_SCANOUT_WAIT_MS = 40;

/*
 * Full-screen startup overlay: title, stage text and progress bar.
 * Every method must be called with the LVGL lock held, except showStage() and
 * stopAnimation(). The spinner redraws every frame and each flush waits for vsync
 * under the LVGL lock, so app_main may wait a long time for that lock while it
 * spins. These two only publish a request that a timer applies in the LVGL task.
 */
class BootLoadingUi {
public:
    static constexpr uint32_t READY_HOLD_MS = 300;
    static constexpr uint32_t FADE_OUT_TIME_MS = 240;

    bool create();
    void setStage(const char *status, int progress);
    void showError(const char *status);
    /* Show "Ready", then fade out and delete the overlay from the LVGL task. */
    void finish();
    void attachToTopLayer(lv_display_t *display);
    void attachToSystemLayer(lv_display_t *display);
    void destroy();

    /* Lock-free. Publish a stage and wait up to timeout_ms for a frame showing it. */
    bool showStage(const char *status, int progress, uint32_t timeout_ms);
    /* Lock-free. Stop the spinner so the LVGL lock is free again for the caller. */
    void stopAnimation();

private:
    static void onStageTimer(lv_timer_t *timer);
    static void onFrameRendered(lv_event_t *event);

    void stopStageUpdates();
    void clearObjectPointers();
    void attachToLayer(lv_obj_t *parent);

    lv_obj_t *root_ = nullptr;
    lv_obj_t *spinner_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *phase_ = nullptr;
    lv_obj_t *status_ = nullptr;
    lv_obj_t *progress_ = nullptr;

    lv_display_t *display_ = nullptr;
    lv_timer_t *stage_timer_ = nullptr;
    SemaphoreHandle_t stage_rendered_ = nullptr;
    bool stage_applied_ = false;    // Touched only in the LVGL task.
    std::atomic<const char *> pending_status_{nullptr};
    std::atomic<int> pending_progress_{0};
    std::atomic<bool> stop_animation_{false};
};

/*
 * Apply an LVGL change and wait until the LVGL task has rendered a frame with
 * it. Takes the LVGL lock itself; call it without holding the lock.
 */
bool apply_and_wait_for_frame(lv_display_t *display, const std::function<void()> &change,
                              uint32_t timeout_ms = BOOT_FRAME_WAIT_MS);

/*
 * Draw the stage before the caller starts its work. Most stages hold the LVGL
 * lock while they run, so the LVGL task would otherwise only render the stage
 * after it has already finished. Call it without holding the lock.
 */
void refresh_boot_loading(BootLoadingUi &loading, const char *status, int progress);

/* Ramp the backlight up to target_percent so power-on does not flash. */
void fade_in_backlight(int target_percent);

/* Shows the startup error on the overlay unless dismissed before startup returns. */
class BootLoadingFailureGuard {
public:
    BootLoadingFailureGuard(BootLoadingUi &loading, lv_display_t *display)
        : loading_(loading), display_(display)
    {
    }

    ~BootLoadingFailureGuard();

    BootLoadingFailureGuard(const BootLoadingFailureGuard &) = delete;
    BootLoadingFailureGuard &operator=(const BootLoadingFailureGuard &) = delete;

    void dismiss()
    {
        armed_ = false;
    }

private:
    BootLoadingUi &loading_;
    lv_display_t *display_;
    bool armed_ = true;
};

/*
 * Owns the Phone until startup succeeds. On failure it moves the overlay back
 * to the top layer, then deletes the Phone under the display lock.
 */
class PhoneStartupOwner {
public:
    using Phone = esp_brookesia::systems::phone::Phone;

    PhoneStartupOwner(Phone *phone, BootLoadingUi &loading, lv_display_t *display)
        : phone_(phone), loading_(loading), display_(display)
    {
    }

    ~PhoneStartupOwner()
    {
        reset();
    }

    PhoneStartupOwner(const PhoneStartupOwner &) = delete;
    PhoneStartupOwner &operator=(const PhoneStartupOwner &) = delete;

    Phone *get() const
    {
        return phone_;
    }

    Phone *release()
    {
        Phone *phone = phone_;
        phone_ = nullptr;
        return phone;
    }

private:
    void reset();

    Phone *phone_;
    BootLoadingUi &loading_;
    lv_display_t *display_;
};
