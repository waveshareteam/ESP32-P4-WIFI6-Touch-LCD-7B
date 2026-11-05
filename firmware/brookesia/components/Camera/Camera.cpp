/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Camera.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"

#include "esp_cache.h"
#include "esp_lib_utils.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "hal/efuse_hal.h"
#include "driver/jpeg_encode.h"
#include "linux/videodev2.h"
#include "freertos/semphr.h"

#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Camera"

LV_IMG_DECLARE(img_app_camera);

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

namespace esp_brookesia::apps {
// The PPA cover-fills the LCD panel with the camera's native frame. When
// frame and panel orientations differ, the frame is turned a quarter turn.
// Three hardware rules drive the geometry:
//   * the SRM scale factor is quantised to 1/16 steps, so a plain float ratio
//     leaves black bands;
//   * the scaled output block must stay inside out.pic_w x out.pic_h, so the
//     picture cannot simply be over-scaled and clipped;
//   * the (rotated) block is placed at the output offset, so an exact-size
//     block at (0,0) covers the whole output picture whatever the anchor is.
// The crop is therefore derived from the quantised factor so that the scaled
// output is exactly the panel size.
static constexpr uint32_t CAMERA_PPA_SCALE_STEPS_PER_UNIT = 16;
// Upscale limit searched when no factor gives an exact panel-size output.
static constexpr uint32_t CAMERA_PPA_SCALE_SEARCH_STEPS = 16;

struct CaptureRequest {
    Camera *camera;
    uint8_t **data;
    size_t *length;
    esp_err_t ret;
    SemaphoreHandle_t done;
};

struct CoverCropConfig {
    uint32_t crop_x;
    uint32_t crop_y;
    uint32_t crop_w;
    uint32_t crop_h;
    float scale_x;
    float scale_y;
    ppa_srm_rotation_angle_t rotation;
};

// Number of source pixels that the hardware scales to exactly `dst` pixels
// at `steps`/16, or 0 when no such count exists. The PPA truncates the
// scaled size, so the smallest count that reaches `dst` is the only one.
static uint32_t exactKeep(uint32_t dst, uint32_t steps)
{
    constexpr uint32_t STEPS = CAMERA_PPA_SCALE_STEPS_PER_UNIT;
    const uint64_t need = static_cast<uint64_t>(dst) * STEPS;
    const uint32_t keep = static_cast<uint32_t>((need + steps - 1) / steps);
    return (static_cast<uint64_t>(keep) * steps / STEPS == dst) ? keep : 0;
}

static CoverCropConfig computeCoverCrop(uint32_t src_w, uint32_t src_h, uint32_t dst_w, uint32_t dst_h)
{
    constexpr uint32_t STEPS = CAMERA_PPA_SCALE_STEPS_PER_UNIT;
    CoverCropConfig crop = {
        .crop_x = 0,
        .crop_y = 0,
        .crop_w = src_w,
        .crop_h = src_h,
        .scale_x = 0.0f,
        .scale_y = 0.0f,
        .rotation = PPA_SRM_ROTATION_ANGLE_0,
    };

    if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        return crop;
    }

    // Turn a portrait frame onto a landscape panel (and vice versa). After
    // the quarter turn the panel width is covered by the source height. A
    // square frame has no orientation and is never turned.
    const bool rotate = (src_w != src_h) && ((src_h > src_w) != (dst_h > dst_w));
    crop.rotation = rotate ? PPA_SRM_ROTATION_ANGLE_270 : PPA_SRM_ROTATION_ANGLE_0;
    const uint32_t along_w = rotate ? src_h : src_w;
    const uint32_t along_h = rotate ? src_w : src_h;

    // Smallest 1/16 step that covers both panel directions.
    const uint64_t need_w = static_cast<uint64_t>(dst_w) * STEPS;
    const uint64_t need_h = static_cast<uint64_t>(dst_h) * STEPS;
    uint32_t steps = LV_MAX(static_cast<uint32_t>((need_w + along_w - 1) / along_w),
                            static_cast<uint32_t>((need_h + along_h - 1) / along_h));
    steps = steps ? steps : 1;

    // Above 1:1 one source pixel spans more than one output pixel, so a step
    // may have no exact crop; take the next step that has one.
    uint32_t keep_along_w = 0;
    uint32_t keep_along_h = 0;
    for (uint32_t s = steps; s <= steps + CAMERA_PPA_SCALE_SEARCH_STEPS; s++) {
        const uint32_t kw = exactKeep(dst_w, s);
        const uint32_t kh = exactKeep(dst_h, s);
        if (kw && kh && kw <= along_w && kh <= along_h) {
            steps = s;
            keep_along_w = kw;
            keep_along_h = kh;
            break;
        }
    }
    if (!keep_along_w) {
        // No exact fit: stay inside the panel and accept a 1px black edge.
        keep_along_w = LV_MIN(static_cast<uint32_t>(need_w / steps), along_w);
        keep_along_h = LV_MIN(static_cast<uint32_t>(need_h / steps), along_h);
    }
    keep_along_w = keep_along_w ? keep_along_w : 1;
    keep_along_h = keep_along_h ? keep_along_h : 1;

    const uint32_t keep_w = rotate ? keep_along_h : keep_along_w;
    const uint32_t keep_h = rotate ? keep_along_w : keep_along_h;
    crop.crop_x = (src_w - keep_w) / 2;
    crop.crop_y = (src_h - keep_h) / 2;
    crop.crop_w = keep_w;
    crop.crop_h = keep_h;
    crop.scale_x = static_cast<float>(steps) / static_cast<float>(STEPS);
    crop.scale_y = crop.scale_x;

    return crop;
}

// Plan the cover crop directly on the native panel. The camera is fixed to the panel, so the
// frame orientation does not depend on the rotation LVGL applies to the UI. The 7B sensor frame
// goes onto the native panel without an extra half turn.
static CoverCropConfig computePanelCrop(uint32_t src_w, uint32_t src_h, const board_display_info_t &panel)
{
    return computeCoverCrop(src_w, src_h, panel.width, panel.height);
}

Camera *Camera::_instance = nullptr;
std::atomic<bool> Camera::_camera_busy{false};

Camera *Camera::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new Camera(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

Camera::Camera(bool use_status_bar, bool use_navigation_bar) : App("Camera", &img_app_camera, true, use_status_bar, use_navigation_bar)
{
}

Camera::~Camera()
{
    close();
}

bool Camera::run(void)
{
    ESP_UTILS_LOGD("Run");

    _close_requested.store(false);
    _close_notification_posted.store(false);
    if (!createCloseRequestTimer()) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Create camera close request timer failed");
        return false;
    }

    board_display_lock(-1);
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
    _status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(_status_label, "Camera");
    lv_obj_set_width(_status_label, lv_pct(100));
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(_status_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(_status_label, LV_ALIGN_CENTER, 0, 0);
    board_display_unlock();

    if (!startPreview()) {
        board_display_lock(-1);
        if (_status_label) {
            lv_label_set_text(_status_label, "camera error");
        }
        board_display_unlock();
    }

    return true;
}

bool Camera::back(void)
{
    ESP_UTILS_LOGD("Back");
    ESP_UTILS_CHECK_FALSE_RETURN(requestStopPreview(), false, "Stop camera preview failed");
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool Camera::close()
{
    ESP_UTILS_LOGD("Close");
    if (!requestStopPreview()) {
        return false;
    }

    destroyCloseRequestTimer();

    board_display_lock(-1);
    if (_status_label) {
        lv_obj_del(_status_label);
        _status_label = nullptr;
    }
    board_display_unlock();
    return true;
}

bool Camera::init()
{
    ESP_UTILS_LOGD("Init");
    return true;
}

bool Camera::deinit()
{
    ESP_UTILS_LOGD("Deinit");
    return true;
}

bool Camera::pause()
{
    ESP_UTILS_LOGD("Pause");
    return requestStopPreview();
}

bool Camera::resume()
{
    ESP_UTILS_LOGD("Resume");
    return startPreview();
}

bool Camera::startPreview()
{
    _preview_desired = true;
    if (_preview_task_handle && _preview_exited.load()) {
        _preview_task_handle = nullptr;
    }
    if (_preview_task_handle) {
        if (!_preview_running.load()) {
            _preview_restart_pending = true;
        }
        return true;
    }
    _preview_restart_pending = false;
    _close_requested.store(false);
    _close_notification_posted.store(false);

    bool expected = false;
    if (!_camera_busy.compare_exchange_strong(expected, true)) {
        ESP_LOGW(ESP_UTILS_LOG_TAG, "Camera is busy");
        return false;
    }

    _preview_exited.store(false);
    _preview_running = true;
    BaseType_t ret = xTaskCreatePinnedToCore(
                         previewTaskEntry,
                         "camera_preview",
                         PREVIEW_TASK_STACK_SIZE,
                         this,
                         PREVIEW_TASK_PRIORITY,
                         &_preview_task_handle,
                         CAMERA_STREAM_CORE);

    if (ret != pdPASS) {
        _camera_busy.store(false);
        _preview_running = false;
        _preview_exited.store(true);
        _preview_task_handle = nullptr;
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Create camera preview task failed");
        return false;
    }

    return true;
}

bool Camera::createCloseRequestTimer()
{
    if (_close_request_timer) {
        return true;
    }
    _close_request_timer = lv_timer_create(closeRequestTimerCallback, 20, this);
    return _close_request_timer != nullptr;
}

void Camera::destroyCloseRequestTimer()
{
    if (_close_request_timer) {
        lv_timer_delete(_close_request_timer);
        _close_request_timer = nullptr;
    }
}

bool Camera::requestStopPreview()
{
    // Called from LVGL callbacks: the worker needs the same lock while
    // restoring dummy draw, so never wait here for it to exit.
    _preview_desired = false;
    _preview_restart_pending = false;
    _close_requested.store(false);
    _preview_running.store(false);
    return true;
}

esp_err_t Camera::loadDisplayFormat()
{
    ESP_RETURN_ON_ERROR(board_display_get_info(&_display_info), ESP_UTILS_LOG_TAG, "Get display info failed");

    switch (_display_info.bits_per_pixel) {
    case 16:
        _lcd_color_mode = PPA_SRM_COLOR_MODE_RGB565;
        _lcd_bytes_per_pixel = 2;
        break;
    case 24:
        _lcd_color_mode = PPA_SRM_COLOR_MODE_RGB888;
        _lcd_bytes_per_pixel = 3;
        break;
    default:
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Unsupported LCD color depth: %u bpp", _display_info.bits_per_pixel);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

esp_err_t Camera::initVideoDriver()
{
    if (_video_driver_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(loadDisplayFormat(), ESP_UTILS_LOG_TAG, "Load display format failed");

    // The board manager camera device runs esp_video_init() with the shared i2c_master for SCCB.
    esp_err_t ret = board_camera_init(&_video_dev_path);
    if (ret != ESP_OK) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Camera driver init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    _video_driver_initialized = true;
    return ESP_OK;
}

esp_err_t Camera::openVideoDevice()
{
    if (_video_fd >= 0) {
        return ESP_OK;
    }

    _video_fd = open(_video_dev_path, O_RDONLY);
    if (_video_fd < 0) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Open %s failed", _video_dev_path);
        return ESP_FAIL;
    }

    struct v4l2_capability capability = {};
    if (ioctl(_video_fd, VIDIOC_QUERYCAP, &capability) != 0) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QUERYCAP failed");
        ::close(_video_fd);
        _video_fd = -1;
        return ESP_FAIL;
    }

    struct v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(_video_fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_G_FMT failed");
        ::close(_video_fd);
        _video_fd = -1;
        return ESP_FAIL;
    }
    if (format.fmt.pix.width == 0 || format.fmt.pix.height == 0) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Camera returned an invalid capture size");
        ::close(_video_fd);
        _video_fd = -1;
        return ESP_ERR_INVALID_SIZE;
    }

    // Keep the node's default format: esp_video picks one the ISP outputs directly, so the CSI
    // passes it through. Requesting the panel format instead (VIDIOC_S_FMT) can make the CSI
    // convert colors, which ESP32-P4 below v3.0 does not support; the PPA converts instead.
    _camera_fmt = format.fmt.pix.pixelformat;
    switch (_camera_fmt) {
    case V4L2_PIX_FMT_RGB565:
        _camera_color_mode = PPA_SRM_COLOR_MODE_RGB565;
        _camera_jpeg_format = JPEG_ENCODE_IN_FORMAT_RGB565;
        _camera_bytes_per_pixel = 2;
        break;
    case V4L2_PIX_FMT_RGB24:
        _camera_color_mode = PPA_SRM_COLOR_MODE_RGB888;
        _camera_jpeg_format = JPEG_ENCODE_IN_FORMAT_RGB888;
        _camera_bytes_per_pixel = 3;
        break;
    default:
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Unsupported camera pixel format: %.4s",
                 reinterpret_cast<const char *>(&_camera_fmt));
        ::close(_video_fd);
        _video_fd = -1;
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_LOGI(
        ESP_UTILS_LOG_TAG,
        "Camera %" PRIu32 "x%" PRIu32 " %.4s, panel %u bpp, chip rev v%u.%u",
        format.fmt.pix.width,
        format.fmt.pix.height,
        reinterpret_cast<const char *>(&_camera_fmt),
        _display_info.bits_per_pixel,
        efuse_hal_chip_revision() / 100,
        efuse_hal_chip_revision() % 100
    );

    _camera_width = format.fmt.pix.width;
    _camera_height = format.fmt.pix.height;
    _camera_buffer_size = format.fmt.pix.sizeimage;
    if (_camera_buffer_size == 0 && format.fmt.pix.bytesperline != 0) {
        _camera_buffer_size = format.fmt.pix.bytesperline * _camera_height;
    }
    if (_camera_buffer_size == 0) {
        _camera_buffer_size = _camera_width * _camera_height * _camera_bytes_per_pixel;
    }

    ESP_LOGI(
        ESP_UTILS_LOG_TAG,
        "Camera format: %" PRIu32 "x%" PRIu32 ", buffer: %u bytes",
        _camera_width,
        _camera_height,
        static_cast<unsigned>(_camera_buffer_size)
    );

    return ESP_OK;
}

esp_err_t Camera::setupCameraBuffers()
{
#ifdef CONFIG_CACHE_L2_CACHE_LINE_SIZE
    _data_cache_line_size = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
#else
    _data_cache_line_size = 64;
#endif

    struct v4l2_requestbuffers req = {};
    req.count = CAMERA_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (ioctl(_video_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_REQBUFS failed");
        return ESP_FAIL;
    }

    for (int i = 0; i < CAMERA_BUFFER_COUNT; i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index = i;

        if (ioctl(_video_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QUERYBUF %d failed", i);
            return ESP_FAIL;
        }

        size_t alloc_size = buf.length ? buf.length : _camera_buffer_size;
        _camera_buffer_lengths[i] = alloc_size;
        _camera_buffers[i] = heap_caps_aligned_calloc(_data_cache_line_size, 1, alloc_size, MALLOC_CAP_SPIRAM);
        if (!_camera_buffers[i]) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Camera buffer %d allocation failed", i);
            return ESP_ERR_NO_MEM;
        }

        buf.m.userptr = (unsigned long)_camera_buffers[i];
        buf.length = alloc_size;

        if (ioctl(_video_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QBUF %d failed", i);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t Camera::captureJpeg(uint8_t **data, size_t *length)
{
    if (xPortGetCoreID() == CAMERA_STREAM_CORE) {
        return captureJpegOnStreamCore(data, length);
    }

    // Callers such as the Xiaozhi MCP tools may run on the other core; hand the capture over.
    CaptureRequest request = {this, data, length, ESP_FAIL, xSemaphoreCreateBinary()};
    ESP_RETURN_ON_FALSE(request.done != nullptr, ESP_ERR_NO_MEM, ESP_UTILS_LOG_TAG, "Create capture semaphore failed");
    BaseType_t created = xTaskCreatePinnedToCore(
                             captureTaskEntry,
                             "camera_capture",
                             PREVIEW_TASK_STACK_SIZE,
                             &request,
                             uxTaskPriorityGet(nullptr),
                             nullptr,
                             CAMERA_STREAM_CORE);
    if (created != pdPASS) {
        vSemaphoreDelete(request.done);
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Create camera capture task failed");
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(request.done, portMAX_DELAY);
    vSemaphoreDelete(request.done);
    return request.ret;
}

esp_err_t Camera::captureJpegOnStreamCore(uint8_t **data, size_t *length)
{
    if (!data || !length) {
        return ESP_ERR_INVALID_ARG;
    }
    *data = nullptr;
    *length = 0;
    bool expected = false;
    if (!_camera_busy.compare_exchange_strong(expected, true)) {
        return ESP_ERR_INVALID_STATE;
    }

    const auto log_dma_heap = [](const char *stage) {
        constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
        ESP_LOGI(ESP_UTILS_LOG_TAG, "Capture %s: internal DMA free=%u largest=%u",
                 stage, static_cast<unsigned>(heap_caps_get_free_size(caps)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(caps)));
    };
    log_dma_heap("before init");

    esp_err_t ret = initVideoDriver();
    bool streaming = false;
    jpeg_encoder_handle_t encoder = nullptr;
    uint8_t *output = nullptr;
    size_t output_size = 0;
    uint32_t encoded_size = 0;
    struct v4l2_buffer frame = {};
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ret == ESP_OK) {
        ret = openVideoDevice();
    }
    log_dma_heap("after init");
    if (ret == ESP_OK) {
        struct timeval timeout = {};
        timeout.tv_sec = 3;
        if (ioctl(_video_fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
            ret = ESP_FAIL;
        }
    }
    if (ret == ESP_OK) {
        ret = setupCameraBuffers();
    }
    log_dma_heap("after buffers");
    if (ret == ESP_OK) {
        if (ioctl(_video_fd, VIDIOC_STREAMON, &type) != 0) {
            ret = ESP_FAIL;
        } else {
            streaming = true;
        }
    }
    // The stream starts at the sensor's initial exposure, so the first frames come out black.
    // Requeue frames for CAPTURE_WARMUP_MS (and at least CAPTURE_WARMUP_MIN_FRAMES) before keeping one.
    const TickType_t warmup_start = xTaskGetTickCount();
    int dropped_frames = 0;
    while (ret == ESP_OK) {
        frame = {};
        frame.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        frame.memory = V4L2_MEMORY_USERPTR;
        if (ioctl(_video_fd, VIDIOC_DQBUF, &frame) != 0) {
            ret = ESP_FAIL;
            break;
        }
        if (frame.index >= CAMERA_BUFFER_COUNT) {
            ret = ESP_ERR_INVALID_STATE;
            break;
        }
        if (dropped_frames >= CAPTURE_WARMUP_MIN_FRAMES &&
                xTaskGetTickCount() - warmup_start >= pdMS_TO_TICKS(CAPTURE_WARMUP_MS)) {
            break;
        }
        frame.m.userptr = (unsigned long)_camera_buffers[frame.index];
        frame.length = _camera_buffer_lengths[frame.index];
        if (ioctl(_video_fd, VIDIOC_QBUF, &frame) != 0) {
            ret = ESP_FAIL;
            break;
        }
        dropped_frames++;
    }
    if (ret == ESP_OK) {
        ESP_LOGI(ESP_UTILS_LOG_TAG, "Capture kept frame after %d warm-up frames", dropped_frames);
    }
    jpeg_encode_engine_cfg_t engine_cfg = {};
    engine_cfg.timeout_ms = 1000;
    if (ret == ESP_OK) {
        ret = jpeg_new_encoder_engine(&engine_cfg, &encoder);
    }
    log_dma_heap("after JPEG init");
    jpeg_encode_memory_alloc_cfg_t mem_cfg = {};
    mem_cfg.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
    if (ret == ESP_OK) {
        output = static_cast<uint8_t *>(jpeg_alloc_encoder_mem(_camera_buffer_size, &mem_cfg, &output_size));
        if (!output) {
            ret = ESP_ERR_NO_MEM;
        }
    }
    jpeg_encode_cfg_t config = {};
    config.width = _camera_width;
    config.height = _camera_height;
    config.src_type = _camera_jpeg_format;
    config.sub_sample = JPEG_DOWN_SAMPLING_YUV422;
    config.image_quality = 75;
    if (ret == ESP_OK) {
        ret = jpeg_encoder_process(encoder, &config, static_cast<const uint8_t *>(_camera_buffers[frame.index]), _camera_buffer_size, output, output_size, &encoded_size);
    }
    if (encoder) {
        jpeg_del_encoder_engine(encoder);
    }
    if (streaming) {
        ioctl(_video_fd, VIDIOC_STREAMOFF, &type);
    }
    releaseCameraBuffers();
    log_dma_heap("after cleanup");
    _camera_busy.store(false);
    if (ret == ESP_OK) {
        *data = output;
        *length = encoded_size;
    } else {
        free(output);
    }
    return ret;
}

esp_err_t Camera::startDummyPreview()
{
    _display = lv_display_get_default();
    ESP_RETURN_ON_FALSE(_display != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "LVGL display is not ready");

    _display_panel = board_display_get_panel_handle();
    ESP_RETURN_ON_FALSE(_display_panel != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "LCD panel is not ready");

    // The preview flips between DPI frame buffers, so it needs at least two of them.
    ESP_RETURN_ON_FALSE(_display_info.frame_buffer_num >= 2, ESP_ERR_NOT_SUPPORTED, ESP_UTILS_LOG_TAG, "Camera preview needs at least two LCD frame buffers, got %u", _display_info.frame_buffer_num);
    // Only the first frame_buffer_num pointers are written.
    esp_err_t ret = esp_lcd_dpi_panel_get_frame_buffer(_display_panel, _display_info.frame_buffer_num, &_lcd_buffers[0], &_lcd_buffers[1], &_lcd_buffers[2]);
    ESP_RETURN_ON_ERROR(ret, ESP_UTILS_LOG_TAG, "Get LCD frame buffers failed");
    _current_lcd_buffer_index = 0;

    if (_ppa_srm_handle == nullptr) {
        ppa_client_config_t ppa_srm_config = {
            .oper_type = PPA_OPERATION_SRM,
        };
        ESP_RETURN_ON_ERROR(
            ppa_register_client(&ppa_srm_config, &_ppa_srm_handle),
            ESP_UTILS_LOG_TAG,
            "Register PPA SRM failed"
        );
    }

    ret = esp_lv_adapter_set_dummy_draw(_display, true);
    if (ret != ESP_OK) {
        stopDummyPreview();
        return ret;
    }
    _dummy_enabled = true;

    // Dummy draw only skips LVGL's flush, so the LVGL task keeps polling the touch input device.
    // The preview loop reads the swipe from that pointer device instead of the touch driver.
    for (lv_indev_t *indev = lv_indev_get_next(nullptr); indev != nullptr; indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            _touch_indev = indev;
            break;
        }
    }
    if (_touch_indev == nullptr) {
        ESP_LOGW(ESP_UTILS_LOG_TAG, "No LVGL touch input, swipe-to-exit is unavailable");
    }

    // Clear and flush every framebuffer once so the first published frame
    // never shows stale panel content.
    const size_t lcd_buffer_size =
        static_cast<size_t>(_display_info.width) * _display_info.height * _lcd_bytes_per_pixel;
    for (int i = 0; i < _display_info.frame_buffer_num; i++) {
        memset(_lcd_buffers[i], 0, lcd_buffer_size);
        ret = esp_cache_msync(_lcd_buffers[i], lcd_buffer_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        if (ret != ESP_OK) {
            stopDummyPreview();
            return ret;
        }
    }

    const CoverCropConfig crop = computePanelCrop(_camera_width, _camera_height, _display_info);

    ESP_LOGI(
        ESP_UTILS_LOG_TAG,
        "Camera preview cover-fills %" PRIu32 "x%" PRIu32 " rotated %ddeg: block %" PRIu32 ",%" PRIu32
        " %" PRIu32 "x%" PRIu32 " scale %.4f on %" PRIu32 "x%" PRIu32 ", %u frame buffers",
        _camera_width,
        _camera_height,
        static_cast<int>(crop.rotation) * 90,
        crop.crop_x,
        crop.crop_y,
        crop.crop_w,
        crop.crop_h,
        crop.scale_x,
        _display_info.width,
        _display_info.height,
        _display_info.frame_buffer_num
    );
    return ESP_OK;
}

void Camera::stopDummyPreview()
{
    _touch_indev = nullptr;
    _touch_active = false;

    if (_dummy_enabled && _display) {
        esp_lv_adapter_set_dummy_draw(_display, false);
        _dummy_enabled = false;
    }

    memset(_lcd_buffers, 0, sizeof(_lcd_buffers));
    _display_panel = nullptr;
    _display = nullptr;
    _current_lcd_buffer_index = 0;
}

void Camera::releaseCameraBuffers()
{
    for (int i = 0; i < CAMERA_BUFFER_COUNT; i++) {
        if (_camera_buffers[i]) {
            heap_caps_free(_camera_buffers[i]);
            _camera_buffers[i] = nullptr;
        }
        _camera_buffer_lengths[i] = 0;
    }

    if (_video_fd >= 0) {
        ::close(_video_fd);
        _video_fd = -1;
    }
}

bool Camera::shouldExitBySwipe()
{
    if (!_touch_indev) {
        return false;
    }

    // The LVGL task updates the input device state; a short lock keeps the preview loop moving.
    if (!board_display_lock(TOUCH_LOCK_TIMEOUT_MS)) {
        return false;
    }
    const bool pressed = lv_indev_get_state(_touch_indev) == LV_INDEV_STATE_PRESSED;
    lv_point_t point = {};
    lv_indev_get_point(_touch_indev, &point);
    board_display_unlock();

    if (!pressed) {
        _touch_active = false;
        return false;
    }

    if (!_touch_active) {
        _touch_active = true;
        _touch_start_x = point.x;
        _touch_start_y = point.y;
        return false;
    }

    int dx = abs((int)point.x - (int)_touch_start_x);
    int dy = abs((int)point.y - (int)_touch_start_y);
    return dx > SWIPE_EXIT_THRESHOLD || dy > SWIPE_EXIT_THRESHOLD;
}

esp_err_t Camera::handleFrame()
{
    const int lcd_buffer_index = (_current_lcd_buffer_index + 1) % _display_info.frame_buffer_num;
    ESP_RETURN_ON_FALSE(_lcd_buffers[lcd_buffer_index] != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "LCD frame buffer %d is not ready", lcd_buffer_index);

    struct v4l2_buffer v4l2_buf = {};
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_USERPTR;

    if (ioctl(_video_fd, VIDIOC_DQBUF, &v4l2_buf) != 0) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_DQBUF failed");
        return ESP_FAIL;
    }
    if (v4l2_buf.index >= CAMERA_BUFFER_COUNT) {
        ESP_LOGE(
            ESP_UTILS_LOG_TAG,
            "Camera returned invalid buffer index: %u",
            static_cast<unsigned>(v4l2_buf.index)
        );
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t display_w = _display_info.width;
    const uint32_t display_h = _display_info.height;
    const CoverCropConfig crop = computePanelCrop(_camera_width, _camera_height, _display_info);
    ESP_RETURN_ON_FALSE((crop.scale_x > 0.0f) && (crop.scale_y > 0.0f),
                        ESP_ERR_INVALID_SIZE, ESP_UTILS_LOG_TAG, "Invalid camera fit");

    ppa_srm_oper_config_t srm_config = {};
    srm_config.in.buffer = _camera_buffers[v4l2_buf.index];
    srm_config.in.pic_w = _camera_width;
    srm_config.in.pic_h = _camera_height;
    srm_config.in.block_w = crop.crop_w;
    srm_config.in.block_h = crop.crop_h;
    srm_config.in.block_offset_x = crop.crop_x;
    srm_config.in.block_offset_y = crop.crop_y;
    srm_config.in.srm_cm = _camera_color_mode;

    srm_config.out.buffer = _lcd_buffers[lcd_buffer_index];
    srm_config.out.buffer_size = ALIGN_UP(
                                     display_w * display_h * _lcd_bytes_per_pixel,
                                     _data_cache_line_size
                                 );
    srm_config.out.pic_w = display_w;
    srm_config.out.pic_h = display_h;
    srm_config.out.block_offset_x = 0;
    srm_config.out.block_offset_y = 0;
    srm_config.out.srm_cm = _lcd_color_mode;

    srm_config.rotation_angle = crop.rotation;
    srm_config.scale_x = crop.scale_x;
    srm_config.scale_y = crop.scale_y;
    // Match the LCD-X/4B/5/Platform camera orientation: the previous 180
    // degree flip (mirror_x = mirror_y = 1) did not correspond to the panel
    // rotation, which the touch correction left unchanged.
    srm_config.mirror_x = 0;
    srm_config.mirror_y = 0;
    srm_config.rgb_swap = 0;
    srm_config.byte_swap = 0;
    srm_config.mode = PPA_TRANS_MODE_BLOCKING;

    esp_err_t ret = ppa_do_scale_rotate_mirror(_ppa_srm_handle, &srm_config);
    if (ret == ESP_OK && _dummy_enabled) {
        ret = esp_lv_adapter_dummy_draw_blit(
                  _display,
                  0,
                  0,
                  display_w,
                  display_h,
                  _lcd_buffers[lcd_buffer_index],
                  true
              );
        if (ret == ESP_OK) {
            _current_lcd_buffer_index = lcd_buffer_index;
        }
    }

    v4l2_buf.m.userptr = (unsigned long)_camera_buffers[v4l2_buf.index];
    v4l2_buf.length = _camera_buffer_lengths[v4l2_buf.index];
    if (ioctl(_video_fd, VIDIOC_QBUF, &v4l2_buf) != 0) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_QBUF failed");
        return ESP_FAIL;
    }

    return ret;
}

void Camera::previewTask()
{
    bool request_close = false;
    esp_err_t ret = initVideoDriver();
    if (ret == ESP_OK) {
        ret = openVideoDevice();
    }
    if (ret == ESP_OK) {
        ret = setupCameraBuffers();
    }
    if (ret == ESP_OK) {
        ret = startDummyPreview();
    }

    if (ret == ESP_OK) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(_video_fd, VIDIOC_STREAMON, &type) != 0) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "VIDIOC_STREAMON failed");
            ret = ESP_FAIL;
        }
    }

    while (_preview_running && ret == ESP_OK) {
        ret = handleFrame();
        if (shouldExitBySwipe()) {
            request_close = true;
            _preview_running = false;
        }
    }

    if (_video_fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(_video_fd, VIDIOC_STREAMOFF, &type);
    }

    stopDummyPreview();
    releaseCameraBuffers();
    _camera_busy.store(false);

    if (request_close) {
        // This worker is pinned to core 0. The GUI-owned timer observes
        // this request from LVGL's context before it calls notifyCoreClosed().
        _close_requested.store(true);
    }

    _preview_running = false;
    _preview_exited.store(true);

    vTaskDelete(NULL);
}

void Camera::previewTaskEntry(void *arg)
{
    static_cast<Camera *>(arg)->previewTask();
}

void Camera::captureTaskEntry(void *arg)
{
    auto *request = static_cast<CaptureRequest *>(arg);
    request->ret = request->camera->captureJpegOnStreamCore(request->data, request->length);
    xSemaphoreGive(request->done);
    vTaskDelete(nullptr);
}

void Camera::closeRequestTimerCallback(lv_timer_t *timer)
{
    auto *self = static_cast<Camera *>(lv_timer_get_user_data(timer));
    if (!self) {
        return;
    }
    if (self->_preview_restart_pending && self->_preview_exited.load()) {
        if (!self->startPreview()) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Restart camera preview failed");
        }
        return;
    }
    if (!self->_preview_desired || !self->_preview_exited.load() || !self->_close_requested.load()) {
        return;
    }

    bool expected = false;
    if (!self->_close_notification_posted.compare_exchange_strong(expected, true)) {
        return;
    }
    if (!self->notifyCoreClosed()) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Notify camera close failed");
        self->_close_notification_posted.store(false);
    }
}

// The app is a singleton owned by itself; the registry must not delete it.
ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, Camera, "Camera", []()
{
    return std::shared_ptr<Camera>(Camera::requestInstance(), [](Camera *) {});
})

} // namespace esp_brookesia::apps
