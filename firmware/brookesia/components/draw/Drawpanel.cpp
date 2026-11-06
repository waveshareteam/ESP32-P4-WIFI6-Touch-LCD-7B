/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Drawpanel"
#include "esp_lib_utils.h"
#include "Drawpanel.hpp"

#define APP_NAME "DrawPanel"

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(img_app_drawpanel);

namespace esp_brookesia::apps {

static constexpr int32_t STROKE_WIDTH = 10;
static constexpr uint32_t STROKE_COLOR = 0xFF0000;
static constexpr uint32_t CANVAS_BG_COLOR = 0xFFFFE0;
// Same pixel format as the display so the canvas blit needs no conversion.
static constexpr lv_color_format_t CANVAS_COLOR_FORMAT = LV_COLOR_FORMAT_RGB565;
// Align the RGB565 canvas buffers to the cache line for PPA-compatible blending.
static constexpr size_t CANVAS_MEM_ALIGN = 128;

static void configureFixedDrawObject(lv_obj_t *obj, bool clickable)
{
    if (!obj) {
        return;
    }

    lv_obj_clear_flag(
        obj,
        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
        LV_OBJ_FLAG_SCROLL_ONE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLL_ON_FOCUS |
        LV_OBJ_FLAG_SCROLL_WITH_ARROW | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE
    );
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    if (clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    }
}

// Bounding box of a stroke segment, padded for the round caps and anti-aliasing.
static lv_area_t strokeBounds(const lv_point_t &a, const lv_point_t &b)
{
    const int32_t pad = STROKE_WIDTH / 2 + 1;
    lv_area_t area;
    area.x1 = LV_MIN(a.x, b.x) - pad;
    area.y1 = LV_MIN(a.y, b.y) - pad;
    area.x2 = LV_MAX(a.x, b.x) + pad;
    area.y2 = LV_MAX(a.y, b.y) + pad;
    return area;
}

Drawpanel *Drawpanel::_instance = nullptr;

Drawpanel *Drawpanel::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new Drawpanel(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

Drawpanel::Drawpanel(bool use_status_bar, bool use_navigation_bar) :
    App(APP_NAME, &img_app_drawpanel, true, use_status_bar, use_navigation_bar),
    _canvas(nullptr),
    _draw_buf{},
    _canvas_mem(nullptr),
    _stroke_active(false),
    _last_point{0, 0}
{
}

Drawpanel::~Drawpanel()
{
    releaseCanvas();
}

bool Drawpanel::run(void)
{
    ESP_UTILS_LOGD("Run");

    lv_obj_t *screen = lv_scr_act();
    // The default screen already matches the visual area. Keep it non-scrollable
    // so nothing on it can turn into a scrollable panel when drawing at an edge.
    configureFixedDrawObject(screen, false);
    lv_obj_set_style_bg_color(screen, lv_color_hex(CANVAS_BG_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    const lv_area_t &area = getVisualArea();
    ESP_UTILS_CHECK_FALSE_RETURN(
        createCanvas(screen, lv_area_get_width(&area), lv_area_get_height(&area)), false, "Create canvas failed"
    );

    return true;
}

bool Drawpanel::back(void)
{
    ESP_UTILS_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

bool Drawpanel::close(void)
{
    ESP_UTILS_LOGD("Close");

    // The canvas must outlive close(): the core snapshots the screen for the
    // recents view right after this. It is released in cleanResource().
    _stroke_active = false;

    return true;
}

bool Drawpanel::init()
{
    ESP_UTILS_LOGD("Init");

    return true;
}

bool Drawpanel::deinit()
{
    ESP_UTILS_LOGD("Deinit");

    return true;
}

bool Drawpanel::pause()
{
    ESP_UTILS_LOGD("Pause");

    return true;
}

bool Drawpanel::resume()
{
    ESP_UTILS_LOGD("Resume");

    return true;
}

bool Drawpanel::cleanResource(void)
{
    ESP_UTILS_LOGD("Clean resource");

    // Called by the core just before it deletes the recorded screen.
    releaseCanvas();

    return true;
}

bool Drawpanel::createCanvas(lv_obj_t *parent, int32_t width, int32_t height)
{
    ESP_UTILS_CHECK_FALSE_RETURN(width > 0 && height > 0, false, "Invalid canvas size %dx%d", (int)width, (int)height);

    const uint32_t stride = lv_draw_buf_width_to_stride(width, CANVAS_COLOR_FORMAT);
    const size_t size = (size_t)stride * height;

    _canvas_mem = heap_caps_aligned_alloc(CANVAS_MEM_ALIGN, size, MALLOC_CAP_SPIRAM);
    ESP_UTILS_CHECK_NULL_RETURN(_canvas_mem, false, "Alloc canvas buffer (%zu bytes) failed", size);

    if (lv_draw_buf_init(&_draw_buf, width, height, CANVAS_COLOR_FORMAT, stride, _canvas_mem, size) != LV_RESULT_OK) {
        ESP_UTILS_LOGE("Init draw buffer failed");
        releaseCanvas();
        return false;
    }

    _canvas = lv_canvas_create(parent);
    if (_canvas == nullptr) {
        ESP_UTILS_LOGE("Create canvas failed");
        releaseCanvas();
        return false;
    }
    lv_canvas_set_draw_buf(_canvas, &_draw_buf);
    lv_obj_set_pos(_canvas, 0, 0);
    configureFixedDrawObject(_canvas, true);
    lv_canvas_fill_bg(_canvas, lv_color_hex(CANVAS_BG_COLOR), LV_OPA_COVER);
    lv_obj_add_event_cb(_canvas, touch_event_cb, LV_EVENT_ALL, this);

    ESP_UTILS_LOGI("Canvas %dx%d, %zu bytes in PSRAM", (int)width, (int)height, size);

    return true;
}

void Drawpanel::releaseCanvas(void)
{
    _stroke_active = false;

    // Delete the object before freeing the buffer it still references.
    if (_canvas) {
        lv_obj_t *canvas = _canvas;
        _canvas = nullptr;
        lv_obj_del(canvas);
    }
    if (_canvas_mem) {
        heap_caps_free(_canvas_mem);
        _canvas_mem = nullptr;
    }
}

void Drawpanel::drawDot(const lv_point_t &point)
{
    lv_layer_t layer;
    lv_canvas_init_layer(_canvas, &layer);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(STROKE_COLOR);
    dsc.radius = LV_RADIUS_CIRCLE;

    const int32_t half = STROKE_WIDTH / 2;
    const lv_area_t coords = {
        point.x - half, point.y - half,
        point.x - half + STROKE_WIDTH - 1, point.y - half + STROKE_WIDTH - 1
    };
    lv_draw_rect(&layer, &dsc, &coords);

    flushLayer(layer, strokeBounds(point, point));
}

void Drawpanel::drawSegment(const lv_point_t &from, const lv_point_t &to)
{
    // LVGL skips zero-length lines; nothing to draw or invalidate.
    if (from.x == to.x && from.y == to.y) {
        return;
    }

    lv_layer_t layer;
    lv_canvas_init_layer(_canvas, &layer);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(STROKE_COLOR);
    dsc.width = STROKE_WIDTH;
    dsc.round_start = 1;
    dsc.round_end = 1;
    dsc.p1.x = from.x;
    dsc.p1.y = from.y;
    dsc.p2.x = to.x;
    dsc.p2.y = to.y;
    lv_draw_line(&layer, &dsc);

    flushLayer(layer, strokeBounds(from, to));
}

void Drawpanel::flushLayer(lv_layer_t &layer, const lv_area_t &dirty)
{
    // Same dispatch loop as lv_canvas_finish_layer(), minus its full-object
    // lv_obj_invalidate(): a full-screen redraw per touch sample would starve
    // the touch read and turn strokes into polylines.
    lv_display_t *display = lv_obj_get_display(_canvas);
    while (layer.draw_task_head) {
        lv_draw_dispatch_wait_for_request();
        if (!lv_draw_dispatch_layer(display, &layer)) {
            lv_draw_wait_for_finish();
            lv_draw_dispatch_request();
        }
    }

    lv_area_t canvas_coords;
    lv_obj_get_coords(_canvas, &canvas_coords);
    lv_area_t abs_dirty = dirty;
    lv_area_move(&abs_dirty, canvas_coords.x1, canvas_coords.y1);
    lv_obj_invalidate_area(_canvas, &abs_dirty);
}

void Drawpanel::touch_event_cb(lv_event_t *e)
{
    Drawpanel *app = static_cast<Drawpanel *>(lv_event_get_user_data(e));
    ESP_UTILS_CHECK_NULL_EXIT(app, "Get drawpanel instance failed");

    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        // Whoever deletes the canvas, never touch it again; the buffer is freed
        // separately by releaseCanvas().
        app->_canvas = nullptr;
        app->_stroke_active = false;
        return;
    }
    if (app->_canvas == nullptr) {
        return;
    }

    switch (code) {
    case LV_EVENT_PRESSED:
    case LV_EVENT_PRESSING: {
        lv_indev_t *indev = lv_event_get_indev(e);
        if (indev == nullptr) {
            return;
        }

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        // Canvas-local coordinates; PRESS_LOCK keeps the press on the canvas
        // when the finger slides off, so pin the stroke to the edge.
        lv_area_t canvas_coords;
        lv_obj_get_coords(app->_canvas, &canvas_coords);
        point.x = LV_CLAMP(0, point.x - canvas_coords.x1, lv_area_get_width(&canvas_coords) - 1);
        point.y = LV_CLAMP(0, point.y - canvas_coords.y1, lv_area_get_height(&canvas_coords) - 1);

        if (code == LV_EVENT_PRESSED || !app->_stroke_active) {
            app->drawDot(point);
        } else {
            app->drawSegment(app->_last_point, point);
        }
        app->_last_point = point;
        app->_stroke_active = true;
        break;
    }
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        app->_stroke_active = false;
        break;
    default:
        break;
    }
}

// The app is a singleton owned by itself; the registry must not delete it.
ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, Drawpanel, APP_NAME, []()
{
    return std::shared_ptr<Drawpanel>(Drawpanel::requestInstance(), [](Drawpanel *) {});
})

} // namespace esp_brookesia::apps
