#pragma once

#include <atomic>

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "board.h"
#include "driver/jpeg_types.h"
#include "driver/ppa.h"
#include "esp_lcd_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"

namespace esp_brookesia::apps
{

    class Camera : public systems::phone::App
    {
    public:
        static Camera *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
        esp_err_t captureJpeg(uint8_t **data, size_t *length);
        ~Camera();

    protected:
        Camera(bool use_status_bar, bool use_navigation_bar);

        bool run(void) override;
        bool back(void) override;
        bool close(void) override;
        bool init(void) override;
        bool deinit(void) override;
        bool pause(void) override;
        bool resume(void) override;

    private:
        static Camera *_instance;
        static std::atomic<bool> _camera_busy;

        static constexpr int CAMERA_BUFFER_COUNT = 2;
        static constexpr int PREVIEW_TASK_STACK_SIZE = 8 * 1024;
        static constexpr int PREVIEW_TASK_PRIORITY = 8;
        // The DSI panel creates the shared DW-GDMA interrupt group on this core, and the CSI
        // controller can only join that group from the same core, so every stream starts here.
        static constexpr BaseType_t CAMERA_STREAM_CORE = 0;
        static constexpr int SWIPE_EXIT_THRESHOLD = 180;
        static constexpr int TOUCH_LOCK_TIMEOUT_MS = 10;
        // A still capture starts a fresh stream; drop frames until the ISP auto exposure settles.
        static constexpr int CAPTURE_WARMUP_MS = 1000;
        static constexpr int CAPTURE_WARMUP_MIN_FRAMES = 5;

        lv_obj_t *_status_label = nullptr;
        lv_display_t *_display = nullptr;
        esp_lcd_panel_handle_t _display_panel = nullptr;
        ppa_client_handle_t _ppa_srm_handle = nullptr;
        lv_indev_t *_touch_indev = nullptr;

        TaskHandle_t _preview_task_handle = nullptr;
        lv_timer_t *_close_request_timer = nullptr;
        int _video_fd = -1;
        const char *_video_dev_path = nullptr;
        bool _video_driver_initialized = false;
        // Panel geometry and pixel format, read from the board at driver init.
        board_display_info_t _display_info = {};
        ppa_srm_color_mode_t _lcd_color_mode = PPA_SRM_COLOR_MODE_RGB565;
        uint8_t _lcd_bytes_per_pixel = 0;
        // The camera keeps the format esp_video picks, so the CSI never converts colors (ESP32-P4
        // below v3.0 cannot); the PPA converts to the panel format and the JPEG encoder takes it as is.
        uint32_t _camera_fmt = 0;
        ppa_srm_color_mode_t _camera_color_mode = PPA_SRM_COLOR_MODE_RGB565;
        jpeg_enc_input_format_t _camera_jpeg_format = JPEG_ENCODE_IN_FORMAT_RGB565;
        uint8_t _camera_bytes_per_pixel = 0;
        std::atomic<bool> _preview_running{false};
        std::atomic<bool> _preview_exited{true};
        bool _preview_desired = false;
        bool _preview_restart_pending = false;
        std::atomic<bool> _close_requested{false};
        std::atomic<bool> _close_notification_posted{false};
        bool _dummy_enabled = false;
        bool _touch_active = false;
        int32_t _touch_start_x = 0;
        int32_t _touch_start_y = 0;
        uint32_t _camera_width = 0;
        uint32_t _camera_height = 0;
        size_t _camera_buffer_size = 0;
        size_t _camera_buffer_lengths[CAMERA_BUFFER_COUNT] = {};
        size_t _data_cache_line_size = 64;
        void *_camera_buffers[CAMERA_BUFFER_COUNT] = {};
        void *_lcd_buffers[BOARD_DISPLAY_MAX_FRAME_BUFFERS] = {};
        int _current_lcd_buffer_index = 0;

        bool startPreview();
        bool requestStopPreview();
        bool createCloseRequestTimer();
        void destroyCloseRequestTimer();
        esp_err_t loadDisplayFormat();
        esp_err_t initVideoDriver();
        esp_err_t openVideoDevice();
        esp_err_t setupCameraBuffers();
        esp_err_t startDummyPreview();
        void stopDummyPreview();
        void releaseCameraBuffers();
        bool shouldExitBySwipe();
        esp_err_t handleFrame();
        void previewTask();
        esp_err_t captureJpegOnStreamCore(uint8_t **data, size_t *length);

        static void previewTaskEntry(void *arg);
        static void captureTaskEntry(void *arg);
        static void closeRequestTimerCallback(lv_timer_t *timer);
    };

} // namespace esp_brookesia::apps
