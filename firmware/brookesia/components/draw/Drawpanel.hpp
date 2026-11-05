/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief Drawpanel application for touch-based drawing on the device screen
 *
 * A single full-screen `lv_canvas` backed by a PSRAM draw buffer. Touch moves
 * are rendered as connected line segments straight into that buffer, so the
 * object count stays constant and strokes have no gaps or point limit.
 */
class Drawpanel: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of Drawpanel
     *
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     * @return Drawpanel* Singleton instance pointer
     */
    static Drawpanel *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destroy the Drawpanel object
     *
     */
    ~Drawpanel();

    using systems::phone::App::startRecordResource;
    using systems::phone::App::endRecordResource;

protected:
    /**
     * @brief Construct a new Drawpanel object (private to enforce singleton)
     *
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     */
    Drawpanel(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;

    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;
    bool pause(void) override;
    bool resume(void) override;
    bool cleanResource(void) override;

private:
    static Drawpanel *_instance;

    /**
     * @brief Allocate the PSRAM draw buffer and create the canvas on `parent`
     *
     * @param parent Parent object (the app screen)
     * @param width Canvas width in pixels
     * @param height Canvas height in pixels
     * @return true on success, false if the buffer or the object could not be created
     */
    bool createCanvas(lv_obj_t *parent, int32_t width, int32_t height);

    /**
     * @brief Delete the canvas object and free its draw buffer (safe to call repeatedly)
     */
    void releaseCanvas(void);

    /**
     * @brief Paint a round dot at `point` (used for the first sample of a stroke)
     */
    void drawDot(const lv_point_t &point);

    /**
     * @brief Paint a round-capped line from `from` to `to`
     */
    void drawSegment(const lv_point_t &from, const lv_point_t &to);

    /**
     * @brief Run the queued draw tasks of `layer` and invalidate only `dirty`
     *
     * @param layer Layer initialized by `lv_canvas_init_layer()`
     * @param dirty Area to redraw, in canvas-local coordinates
     */
    void flushLayer(lv_layer_t &layer, const lv_area_t &dirty);

    /**
     * @brief Touch event callback for drawing on the canvas
     *
     * @param e LVGL event data
     */
    static void touch_event_cb(lv_event_t *e);

    lv_obj_t *_canvas;
    lv_draw_buf_t _draw_buf;
    void *_canvas_mem;
    bool _stroke_active;
    lv_point_t _last_point;
};

} // namespace esp_brookesia::apps
