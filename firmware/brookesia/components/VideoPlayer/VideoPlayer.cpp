/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "VideoPlayer.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"

#include "esp_lib_utils.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "esp_extractor_defaults.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>
#include <errno.h>
#include <sys/stat.h>
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:VideoPlayer"

LV_IMG_DECLARE(img_app_vedioplayer);

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
// The PPA SRM scale has a 4-bit fraction: it steps in 1/16.
#define PPA_SCALE_STEPS 16

namespace esp_brookesia::apps
{

    namespace {

    constexpr uint32_t ceil_div(uint32_t num, uint32_t den)
    {
        return (num + den - 1) / den;
    }

    /*
     * TEMPORARY playback diagnostics: per-stage timings, logged every
     * PLAYBACK_STATS_PERIOD_US. Video fields are written by the video task and audio
     * fields by the audio task; the log may read a field mid-update, which is fine here.
     * Remove once the stutter / A-V sync root cause is fixed.
     */
    constexpr int64_t PLAYBACK_STATS_PERIOD_US = 2 * 1000 * 1000;

    struct StageTime {
        int64_t total_us = 0;
        int64_t max_us = 0;

        void add(int64_t us)
        {
            total_us += us;
            max_us = us > max_us ? us : max_us;
        }
    };

    struct PlaybackStats {
        int64_t window_start_us = 0;
        int64_t last_frame_us = 0;
        uint32_t frames = 0;
        uint32_t dropped = 0;
        uint32_t audio_chunks = 0;
        uint64_t audio_bytes = 0;
        uint32_t underruns_at_start = 0;
        StageTime frame_interval;
        StageTime decode;
        StageTime ppa;
        StageTime blit;
        StageTime audio_write;
    };

    PlaybackStats s_stats;

    void playback_stats_reset()
    {
        s_stats = {};
        s_stats.window_start_us = esp_timer_get_time();
        s_stats.underruns_at_start = board_i2s_get_tx_underrun_count();
    }

    void playback_stats_maybe_log()
    {
        const int64_t now = esp_timer_get_time();
        const int64_t elapsed = now - s_stats.window_start_us;
        if (elapsed < PLAYBACK_STATS_PERIOD_US) {
            return;
        }
        // Integer-only formatting: all times are in milliseconds, fps in tenths.
        auto avg_ms = [](const StageTime &t, uint32_t n) {
            return static_cast<unsigned long>(n ? t.total_us / n / 1000 : 0);
        };
        auto max_ms = [](const StageTime &t) {
            return static_cast<unsigned long>(t.max_us / 1000);
        };
        const uint32_t n = s_stats.frames;
        const unsigned long fps_x10 = static_cast<unsigned long>(n * 10000000LL / elapsed);
        ESP_LOGI("VideoStats",
                 "fps=%lu.%lu dropped %lu | interval avg %lu max %lu ms | decode avg %lu max %lu | "
                 "ppa avg %lu max %lu | blit avg %lu max %lu | "
                 "audio %lu chunks %lu B/s, write avg %lu max %lu ms | i2s underruns %lu | "
                 "dma free %lu largest %lu",
                 fps_x10 / 10, fps_x10 % 10, static_cast<unsigned long>(s_stats.dropped),
                 avg_ms(s_stats.frame_interval, n), max_ms(s_stats.frame_interval),
                 avg_ms(s_stats.decode, n), max_ms(s_stats.decode),
                 avg_ms(s_stats.ppa, n), max_ms(s_stats.ppa),
                 avg_ms(s_stats.blit, n), max_ms(s_stats.blit),
                 static_cast<unsigned long>(s_stats.audio_chunks),
                 static_cast<unsigned long>(s_stats.audio_bytes * 1000000ULL / elapsed),
                 avg_ms(s_stats.audio_write, s_stats.audio_chunks), max_ms(s_stats.audio_write),
                 static_cast<unsigned long>(board_i2s_get_tx_underrun_count() - s_stats.underruns_at_start),
                 static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)),
                 static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)));
        const int64_t last_frame_us = s_stats.last_frame_us;
        playback_stats_reset();
        s_stats.last_frame_us = last_frame_us;
    }

    } // namespace

    VideoPlayer *VideoPlayer::_instance = nullptr;

    VideoPlayer *VideoPlayer::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new VideoPlayer(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    VideoPlayer::VideoPlayer(bool use_status_bar, bool use_navigation_bar) : App("VideoPlayer", &img_app_vedioplayer, true, use_status_bar, use_navigation_bar), sd_mounted(false)
    {
        display = nullptr;
        display_panel = nullptr;
        current_buf_idx = 0;
        ppa_srm_handle = nullptr;
        touch_handle = nullptr;
        jpeg_handle = nullptr;
        jpeg_output_buffer = nullptr;
        jpeg_output_buffer_size = 0;
        play_task_handle = nullptr;
        is_paused.store(false);
        loop_playback.store(false);
        dummy_enabled = false;
        lvgl_paused = false;
        touch_active = false;
        avi_file_list = nullptr;
        avi_file_count = 0;
        last_video_width = 0;
        last_video_height = 0;
    }

    VideoPlayer::~VideoPlayer()
    {
        close();
    }

    bool VideoPlayer::run(void)
    {
        ESP_UTILS_LOGD("Run");
        is_paused.store(false);
        close_requested.store(false);
        close_notification_posted.store(false);
        if (!createCloseRequestTimer()) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Create video close request timer failed");
            return false;
        }
        if (!sd_mounted) {
            esp_err_t mount_ret = board_sdcard_mount();
            sd_mounted = mount_ret == ESP_OK || mount_ret == ESP_ERR_INVALID_STATE;
            if (!sd_mounted) {
                ESP_LOGE(
                    ESP_UTILS_LOG_TAG,
                    "Mount %s failed: %s",
                    BOARD_SD_MOUNT_POINT,
                    esp_err_to_name(mount_ret)
                );
            }
        }
        board_display_lock(-1);
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
        status_label = lv_label_create(lv_scr_act());
        lv_label_set_text(status_label, sd_mounted ? "loading files..." : "sd error");
        lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(status_label, lv_pct(100));
        lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_20, 0);
        board_display_unlock();

        if (!sd_mounted)
        {
            return true;
        }

        if (getAviFileList(BOARD_SD_MOUNT_POINT "/avi") != ESP_OK || avi_file_count == 0)
        {
            board_display_lock(-1);
            lv_label_set_text(status_label, "avi error");
            board_display_unlock();
            return true;
        }

        board_display_lock(-1);
        lv_obj_del(status_label);
        status_label = nullptr;
        board_display_unlock();

        return startPlaybackTask();
    }

    bool VideoPlayer::back(void)
    {
        ESP_UTILS_LOGD("Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool VideoPlayer::close()
    {
        ESP_UTILS_LOGD("Close");
        if (!stopPlaybackTask(pdMS_TO_TICKS(2000))) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Playback task did not stop before close");
            return false;
        }

        destroyCloseRequestTimer();
        releasePlaybackResources();

        board_display_lock(-1);
        if (status_label)
        {
            lv_obj_del(status_label);
            status_label = nullptr;
        }
        board_display_unlock();

        if (avi_file_list)
        {
            for (int i = 0; i < avi_file_count; i++)
            {
                free(avi_file_list[i]);
            }
            free(avi_file_list);
            avi_file_list = nullptr;
            avi_file_count = 0;
        }

        return true;
    }

    bool VideoPlayer::init()
    {
        ESP_UTILS_LOGD("Init");
        // Mount lazily in run() so inserting a card after boot is supported and
        // application installation does not initialize SDMMC unnecessarily.
        return true;
    }

    bool VideoPlayer::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool VideoPlayer::pause()
    {
        ESP_UTILS_LOGD("Pause");
        if (!stopPlaybackTask(pdMS_TO_TICKS(2000))) {
            return false;
        }
        releasePlaybackResources();
        return true;
    }

    bool VideoPlayer::resume()
    {
        ESP_UTILS_LOGD("Resume");
        if (!sd_mounted || !avi_file_list || avi_file_count == 0) {
            return true;
        }
        return startPlaybackTask();
    }

    bool VideoPlayer::startPlaybackTask()
    {
        if (play_task_handle && playback_exited.load()) {
            play_task_handle = nullptr;
            releasePlaybackResources();
        }
        if (play_task_handle) {
            return true;
        }
        if (!avi_file_list || avi_file_count == 0) {
            return false;
        }

        loop_playback.store(true);
        is_paused.store(false);
        playback_exited.store(false);

        // Keep the playback control stack out of the internal DMA heap used by SDIO.
        BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(
            playTask,
            "avi_play_task",
            32768,
            this,
            7,
            &play_task_handle,
            0,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );
        if (ret != pdPASS) {
            loop_playback.store(false);
            playback_exited.store(true);
            play_task_handle = nullptr;
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Create playback task failed");
            return false;
        }

        return true;
    }

    bool VideoPlayer::stopPlaybackTask(TickType_t timeout)
    {
        loop_playback.store(false);
        is_paused.store(false);

        if (!play_task_handle) {
            return true;
        }
        if (xTaskGetCurrentTaskHandle() == play_task_handle) {
            return false;
        }

        TickType_t start = xTaskGetTickCount();
        while (!playback_exited.load() && xTaskGetTickCount() - start < timeout) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (!playback_exited.load()) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Timed out waiting for playback task");
            return false;
        }
        play_task_handle = nullptr;
        return true;
    }

    bool VideoPlayer::createCloseRequestTimer()
    {
        if (close_request_timer) {
            return true;
        }
        close_request_timer = lv_timer_create(closeRequestTimerCallback, 20, this);
        return close_request_timer != nullptr;
    }

    void VideoPlayer::destroyCloseRequestTimer()
    {
        if (close_request_timer) {
            lv_timer_delete(close_request_timer);
            close_request_timer = nullptr;
        }
    }

    void VideoPlayer::releasePlaybackResources()
    {
        /* This is GUI-owned and is only called after playback_exited is true. */
        deinitJpegDecoder();
        releaseJpegOutputBuffer();
        deinitDisplayBypass();
        (void)board_codec_output_stop();
        audio_ready.store(false);
    }

    esp_err_t VideoPlayer::getAviFileList(const char *dir_path)
    {
        DIR *dir = opendir(dir_path);
        if (!dir) {
            ESP_LOGW(
                ESP_UTILS_LOG_TAG,
                "Open AVI directory failed: %s (errno=%d)",
                dir_path,
                errno
            );
            return ESP_ERR_NOT_FOUND;
        }

        auto is_regular_avi = [dir_path](const struct dirent *entry) {
            if (!entry) {
                return false;
            }
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext || strcasecmp(ext, ".avi") != 0) {
                return false;
            }
            if (entry->d_type == DT_REG) {
                return true;
            }
            if (entry->d_type != DT_UNKNOWN) {
                return false;
            }

            char path[384] = {};
            int written = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
            if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) {
                return false;
            }
            struct stat info = {};
            return stat(path, &info) == 0 && S_ISREG(info.st_mode);
        };

        int count = 0;
        struct dirent *entry = nullptr;
        while ((entry = readdir(dir)) != nullptr) {
            if (is_regular_avi(entry)) {
                ++count;
            }
        }
        if (count == 0) {
            closedir(dir);
            ESP_LOGW(ESP_UTILS_LOG_TAG, "No AVI files found in %s", dir_path);
            return ESP_ERR_NOT_FOUND;
        }

        avi_file_list = static_cast<char **>(calloc(count, sizeof(char *)));
        if (!avi_file_list) {
            closedir(dir);
            return ESP_ERR_NO_MEM;
        }

        int loaded_count = 0;
        rewinddir(dir);
        while ((entry = readdir(dir)) != nullptr && loaded_count < count) {
            if (!is_regular_avi(entry)) {
                continue;
            }
            size_t len = strlen(dir_path) + strlen(entry->d_name) + 2;
            char *full_path = static_cast<char *>(malloc(len));
            if (!full_path) {
                for (int i = 0; i < loaded_count; ++i) {
                    free(avi_file_list[i]);
                }
                free(avi_file_list);
                avi_file_list = nullptr;
                avi_file_count = 0;
                closedir(dir);
                return ESP_ERR_NO_MEM;
            }
            snprintf(full_path, len, "%s/%s", dir_path, entry->d_name);
            avi_file_list[loaded_count++] = full_path;
        }

        closedir(dir);
        avi_file_count = loaded_count;
        ESP_LOGI(
            ESP_UTILS_LOG_TAG,
            "Loaded %d AVI file(s) from %s",
            avi_file_count,
            dir_path
        );
        return avi_file_count > 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
    }

    esp_err_t VideoPlayer::initDisplayBypass()
    {
#ifdef CONFIG_CACHE_L2_CACHE_LINE_SIZE
        data_cache_line_size = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
#else
        data_cache_line_size = 64;
#endif

        display = lv_display_get_default();
        ESP_RETURN_ON_FALSE(display != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "LVGL display is not ready");

        display_panel = board_display_get_panel_handle();
        ESP_RETURN_ON_FALSE(display_panel != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "LCD panel is not ready");

        ESP_RETURN_ON_ERROR(board_display_get_info(&display_info), ESP_UTILS_LOG_TAG, "Get display info failed");
        // Decode straight into the panel's pixel format so the PPA only crops and scales.
        switch (display_info.bits_per_pixel) {
        case 16:
            bytes_per_pixel = 2;
            ppa_color_mode = PPA_SRM_COLOR_MODE_RGB565;
            jpeg_sw_format = JPEG_PIXEL_FORMAT_RGB565_LE;
            jpeg_hw_format = JPEG_DECODE_OUT_FORMAT_RGB565;
            break;
        case 24:
            bytes_per_pixel = 3;
            ppa_color_mode = PPA_SRM_COLOR_MODE_RGB888;
            jpeg_sw_format = JPEG_PIXEL_FORMAT_RGB888;
            jpeg_hw_format = JPEG_DECODE_OUT_FORMAT_RGB888;
            break;
        default:
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Unsupported LCD color depth: %u bpp", display_info.bits_per_pixel);
            return ESP_ERR_NOT_SUPPORTED;
        }

        // Playback flips between DPI frame buffers, so it needs at least two of them.
        ESP_RETURN_ON_FALSE(display_info.frame_buffer_num >= 2, ESP_ERR_NOT_SUPPORTED, ESP_UTILS_LOG_TAG,
                            "VideoPlayer needs at least two LCD frame buffers, got %u", display_info.frame_buffer_num);
        // Only the first frame_buffer_num pointers are written.
        esp_err_t ret = esp_lcd_dpi_panel_get_frame_buffer(display_panel, display_info.frame_buffer_num,
                                                           &lcd_buffers[0], &lcd_buffers[1], &lcd_buffers[2]);
        ESP_RETURN_ON_ERROR(ret, ESP_UTILS_LOG_TAG, "Get LCD frame buffers failed");
        current_buf_idx = 0;

        if (ppa_srm_handle == nullptr) {
            ppa_client_config_t ppa_srm_config = {
                .oper_type = PPA_OPERATION_SRM,
            };
            ESP_RETURN_ON_ERROR(
                ppa_register_client(&ppa_srm_config, &ppa_srm_handle),
                ESP_UTILS_LOG_TAG,
                "Register PPA SRM failed"
            );
        }

        ret = esp_lv_adapter_set_dummy_draw(display, true);
        if (ret != ESP_OK) {
            deinitDisplayBypass();
            return ret;
        }
        dummy_enabled = true;

        ret = esp_lv_adapter_pause(-1);
        if (ret != ESP_OK) {
            deinitDisplayBypass();
            return ret;
        }
        lvgl_paused = true;

        // LVGL is paused, so the swipe is read from the touch driver it would otherwise poll.
        touch_handle = board_display_get_touch_handle();
        if (touch_handle == nullptr) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "No touch input, swipe-to-exit is unavailable");
            deinitDisplayBypass();
            return ESP_ERR_NOT_FOUND;
        }

        ESP_LOGI(
            ESP_UTILS_LOG_TAG,
            "Video display bypass: %lux%lu, %u frame buffers",
            static_cast<unsigned long>(display_info.width),
            static_cast<unsigned long>(display_info.height),
            display_info.frame_buffer_num
        );
        return ESP_OK;
    }

    void VideoPlayer::deinitDisplayBypass()
    {
        // Stop direct reads before the LVGL adapter resumes using the same
        // board-owned GT911 handle.
        touch_handle = nullptr;
        touch_active = false;

        if (dummy_enabled && display) {
            esp_lv_adapter_set_dummy_draw(display, false);
            dummy_enabled = false;
        }

        if (lvgl_paused) {
            esp_lv_adapter_resume();
            lvgl_paused = false;
        }

        memset(lcd_buffers, 0, sizeof(lcd_buffers));
        display_panel = nullptr;
        display = nullptr;
        current_buf_idx = 0;
        last_video_width = 0;
        last_video_height = 0;
    }

    esp_err_t VideoPlayer::initJpegDecoder()
    {
        if (jpeg_hw_handle == nullptr) {
            jpeg_decode_engine_cfg_t hw_config = {};
            hw_config.intr_priority = 0;
            // Must exceed the decode time of one frame.
            hw_config.timeout_ms = 200;
            esp_err_t ret = jpeg_new_decoder_engine(&hw_config, &jpeg_hw_handle);
            if (ret != ESP_OK) {
                // The software decoder below still works on its own.
                ESP_LOGW(ESP_UTILS_LOG_TAG, "Hardware JPEG decoder unavailable: %s", esp_err_to_name(ret));
                jpeg_hw_handle = nullptr;
            }
        }

        if (jpeg_handle != nullptr)
            return ESP_OK;

        jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
        config.output_type = jpeg_sw_format;

        jpeg_error_t err = jpeg_dec_open(&config, &jpeg_handle);
        return (err == JPEG_ERR_OK) ? ESP_OK : ESP_FAIL;
    }

    void VideoPlayer::deinitJpegDecoder()
    {
        if (jpeg_hw_handle) {
            jpeg_del_decoder_engine(jpeg_hw_handle);
            jpeg_hw_handle = nullptr;
        }
        jpeg_hw_fallback_logged = false;
        if (jpeg_handle)
        {
            jpeg_dec_close(jpeg_handle);
            jpeg_handle = nullptr;
        }
    }

    esp_err_t VideoPlayer::ensureJpegOutputBuffer(int out_len)
    {
        if (out_len <= 0) {
            return ESP_ERR_INVALID_SIZE;
        }

        if (jpeg_output_buffer && jpeg_output_buffer_size >= out_len) {
            return ESP_OK;
        }

        releaseJpegOutputBuffer();
        size_t alloc_size = ALIGN_UP(static_cast<size_t>(out_len), 16);
        jpeg_output_buffer = static_cast<uint8_t *>(
            heap_caps_aligned_calloc(16, 1, alloc_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!jpeg_output_buffer) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "JPEG output buffer allocation failed: %u bytes", static_cast<unsigned>(alloc_size));
            return ESP_ERR_NO_MEM;
        }

        jpeg_output_buffer_size = static_cast<int>(alloc_size);
        return ESP_OK;
    }

    void VideoPlayer::releaseJpegOutputBuffer()
    {
        if (jpeg_output_buffer) {
            heap_caps_free(jpeg_output_buffer);
            jpeg_output_buffer = nullptr;
            jpeg_output_buffer_size = 0;
        }
        if (jpeg_hw_output_buffer) {
            heap_caps_free(jpeg_hw_output_buffer);
            jpeg_hw_output_buffer = nullptr;
            jpeg_hw_output_buffer_size = 0;
        }
    }

    esp_err_t VideoPlayer::decodeFrameHw(const uint8_t *jpeg, uint32_t jpeg_size, DecodedFrame &frame)
    {
        ESP_RETURN_ON_FALSE(jpeg_hw_handle != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "No hardware decoder");

        jpeg_decode_picture_info_t info = {};
        ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(jpeg, jpeg_size, &info), ESP_UTILS_LOG_TAG, "Parse JPEG header failed");
        ESP_RETURN_ON_FALSE(info.width > 0 && info.height > 0, ESP_ERR_INVALID_SIZE, ESP_UTILS_LOG_TAG, "Invalid JPEG size");

        // The hardware writes whole MCUs, so the output is padded to the MCU size.
        uint32_t mcu_w = 8;
        uint32_t mcu_h = 8;
        if (info.sample_method == JPEG_DOWN_SAMPLING_YUV420) {
            mcu_w = 16;
            mcu_h = 16;
        } else if (info.sample_method == JPEG_DOWN_SAMPLING_YUV422) {
            mcu_w = 16;
        }
        const uint32_t pic_w = ALIGN_UP(info.width, mcu_w);
        const uint32_t pic_h = ALIGN_UP(info.height, mcu_h);
        const size_t needed = static_cast<size_t>(pic_w) * pic_h * bytes_per_pixel;

        if (!jpeg_hw_output_buffer || jpeg_hw_output_buffer_size < needed) {
            if (jpeg_hw_output_buffer) {
                heap_caps_free(jpeg_hw_output_buffer);
                jpeg_hw_output_buffer = nullptr;
                jpeg_hw_output_buffer_size = 0;
            }
            // The decoder DMA needs a cache-line aligned buffer address and size.
            jpeg_decode_memory_alloc_cfg_t mem_config = {};
            mem_config.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER;
            size_t allocated = 0;
            jpeg_hw_output_buffer = static_cast<uint8_t *>(jpeg_alloc_decoder_mem(needed, &mem_config, &allocated));
            ESP_RETURN_ON_FALSE(jpeg_hw_output_buffer != nullptr, ESP_ERR_NO_MEM, ESP_UTILS_LOG_TAG,
                                "Allocate %u-byte JPEG output buffer failed", static_cast<unsigned>(needed));
            jpeg_hw_output_buffer_size = allocated;
        }

        jpeg_decode_cfg_t decode_config = {};
        decode_config.output_format = jpeg_hw_format;
        // BGR element order is the little-endian layout the panel and PPA expect.
        decode_config.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
        decode_config.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
        uint32_t out_size = 0;
        ESP_RETURN_ON_ERROR(
            jpeg_decoder_process(jpeg_hw_handle, &decode_config, jpeg, jpeg_size,
                                 jpeg_hw_output_buffer, jpeg_hw_output_buffer_size, &out_size),
            ESP_UTILS_LOG_TAG, "Hardware JPEG decode failed"
        );

        frame = {
            .buffer = jpeg_hw_output_buffer,
            .pic_w = pic_w,
            .pic_h = pic_h,
            .width = info.width,
            .height = info.height,
        };
        return ESP_OK;
    }

    esp_err_t VideoPlayer::decodeFrameSw(const uint8_t *jpeg, uint32_t jpeg_size, DecodedFrame &frame)
    {
        ESP_RETURN_ON_FALSE(jpeg_handle != nullptr, ESP_ERR_INVALID_STATE, ESP_UTILS_LOG_TAG, "No software decoder");

        jpeg_dec_io_t io = {
            .inbuf = const_cast<uint8_t *>(jpeg),
            .inbuf_len = static_cast<int>(jpeg_size),
            .outbuf = nullptr,
        };

        jpeg_dec_header_info_t info;
        ESP_RETURN_ON_FALSE(jpeg_dec_parse_header(jpeg_handle, &io, &info) == JPEG_ERR_OK, ESP_FAIL,
                            ESP_UTILS_LOG_TAG, "JPEG header parse failed");
        ESP_RETURN_ON_FALSE(info.width > 0 && info.height > 0, ESP_ERR_INVALID_SIZE, ESP_UTILS_LOG_TAG,
                            "Invalid JPEG frame size: %dx%d", info.width, info.height);

        int out_len = 0;
        ESP_RETURN_ON_FALSE(jpeg_dec_get_outbuf_len(jpeg_handle, &out_len) == JPEG_ERR_OK &&
                            ensureJpegOutputBuffer(out_len) == ESP_OK, ESP_ERR_NO_MEM,
                            ESP_UTILS_LOG_TAG, "Prepare JPEG output buffer failed");

        io.outbuf = jpeg_output_buffer;
        ESP_RETURN_ON_FALSE(jpeg_dec_process(jpeg_handle, &io) == JPEG_ERR_OK, ESP_FAIL,
                            ESP_UTILS_LOG_TAG, "JPEG decode process failed");

        // The software decoder writes exactly width x height pixels.
        frame = {
            .buffer = jpeg_output_buffer,
            .pic_w = info.width,
            .pic_h = info.height,
            .width = info.width,
            .height = info.height,
        };
        return ESP_OK;
    }

    namespace {

    // The extractor pool holds every frame queued below plus the one being read.
    // 800x1280 MJPEG frames are typically 100-400 KB.
    constexpr uint32_t EXTRACTOR_POOL_SIZE = 4 * 1024 * 1024;
    constexpr uint16_t EXTRACTOR_OUT_ALIGN = 16;
    constexpr size_t FILE_IO_BUFFER_SIZE = 32 * 1024;
    constexpr UBaseType_t VIDEO_QUEUE_LEN = 6;
    constexpr UBaseType_t AUDIO_QUEUE_LEN = 24;
    constexpr TickType_t QUEUE_POLL_TICKS = pdMS_TO_TICKS(20);
    constexpr int64_t SWIPE_POLL_US = 30 * 1000;

    // A video frame later than this behind the master clock is dropped before decoding.
    constexpr int64_t VIDEO_LATE_DROP_MS = 40;
    // Never drop more than this many frames in a row, so the picture keeps moving even
    // when every frame is late.
    constexpr int VIDEO_MAX_CONSECUTIVE_DROPS = 3;
    // Wait this long for the first audio before running video on the system clock.
    constexpr int64_t AUDIO_START_TIMEOUT_MS = 300;
    // An early frame never waits longer than this, which bounds damage from a bad PTS.
    constexpr int64_t VIDEO_MAX_EARLY_WAIT_MS = 500;
    // A write blocking longer than this means the I2S DMA ring was full.
    constexpr int64_t AUDIO_WRITE_BLOCKED_US = 2 * 1000;

    constexpr uint32_t AUDIO_TASK_STACK = 4 * 1024;
    constexpr uint32_t VIDEO_TASK_STACK = 8 * 1024;
    constexpr UBaseType_t AUDIO_TASK_PRIORITY = 9;
    constexpr UBaseType_t VIDEO_TASK_PRIORITY = 6;

    // Input of the extractor: an open file and its size.
    struct FileSource {
        FILE *file = nullptr;
        uint32_t size = 0;
        void *io_buffer = nullptr;
    };

    int extractor_file_read(void *buffer, uint32_t size, void *ctx)
    {
        FILE *file = static_cast<FileSource *>(ctx)->file;
        size_t read = fread(buffer, 1, size, file);
        if (read == 0 && ferror(file)) {
            return -1;
        }
        return static_cast<int>(read);
    }

    int extractor_file_seek(uint32_t position, void *ctx)
    {
        FILE *file = static_cast<FileSource *>(ctx)->file;
        return fseek(file, static_cast<long>(position), SEEK_SET) == 0 ? 0 : -1;
    }

    uint32_t extractor_file_size(void *ctx)
    {
        return static_cast<FileSource *>(ctx)->size;
    }

    void release_frame(esp_extractor_handle_t extractor, esp_extractor_frame_info_t &frame)
    {
        if (frame.frame_buffer) {
            esp_extractor_release_frame(extractor, &frame);
        }
    }

    } // namespace

    struct VideoPlayer::Playback {
        VideoPlayer *player = nullptr;
        FileSource source;
        esp_extractor_handle_t extractor = nullptr;
        QueueHandle_t audio_queue = nullptr;  // esp_extractor_frame_info_t
        QueueHandle_t video_queue = nullptr;  // esp_extractor_frame_info_t
        SemaphoreHandle_t worker_done = nullptr;
        int workers = 0;
        std::atomic<bool> stop{false};

        bool has_audio = false;
        uint32_t audio_bytes_per_sec = 0;
        int64_t audio_dma_ms = 0;
        // Audio task only: estimated I2S DMA fill level after the last write.
        int64_t dma_level_ms = 0;
        int64_t last_write_end_us = 0;

        /*
         * Master clock: the media time playing at clock_anchor_us, advancing in real time
         * but never past clock_limit_ms (the end of the audio written so far). The audio
         * task re-anchors it after every write; without audio it runs on the system clock.
         */
        std::mutex clock_mutex;
        bool clock_valid = false;
        int64_t clock_anchor_us = 0;
        int64_t clock_anchor_ms = 0;
        int64_t clock_limit_ms = INT64_MAX;

        void setClock(int64_t anchor_us, int64_t media_ms, int64_t limit_ms)
        {
            std::lock_guard<std::mutex> lock(clock_mutex);
            clock_valid = true;
            clock_anchor_us = anchor_us;
            clock_anchor_ms = media_ms;
            clock_limit_ms = limit_ms;
        }

        // Lets the clock run past the written audio once the audio track has ended.
        void releaseClockLimit()
        {
            std::lock_guard<std::mutex> lock(clock_mutex);
            clock_limit_ms = INT64_MAX;
        }

        bool clock(int64_t &media_ms)
        {
            std::lock_guard<std::mutex> lock(clock_mutex);
            if (!clock_valid) {
                return false;
            }
            const int64_t ms = clock_anchor_ms + (esp_timer_get_time() - clock_anchor_us) / 1000;
            media_ms = ms < clock_limit_ms ? ms : clock_limit_ms;
            return true;
        }
    };

    void VideoPlayer::renderJpegFrame(const uint8_t *jpeg, uint32_t jpeg_size)
    {
        const int64_t frame_start_us = esp_timer_get_time();
        if (s_stats.last_frame_us != 0) {
            s_stats.frame_interval.add(frame_start_us - s_stats.last_frame_us);
        }
        s_stats.last_frame_us = frame_start_us;
        const uint32_t display_w = display_info.width;
        const uint32_t display_h = display_info.height;
        const int next_buf_idx = (current_buf_idx + 1) % display_info.frame_buffer_num;

        if (initJpegDecoder() != ESP_OK || !lcd_buffers[next_buf_idx] || !dummy_enabled || !ppa_srm_handle) {
            return;
        }

        // Decode each MJPEG frame into a PSRAM buffer, then use PPA SRM to scale it
        // into the next LCD frame buffer without involving LVGL widgets.
        DecodedFrame frame = {};
        esp_err_t decode_ret = decodeFrameHw(jpeg, jpeg_size, frame);
        if (decode_ret != ESP_OK) {
            // e.g. progressive JPEG, which the hardware decoder does not support.
            if (!jpeg_hw_fallback_logged) {
                ESP_LOGW(ESP_UTILS_LOG_TAG, "Hardware JPEG decode failed (%s), using software decoder",
                         esp_err_to_name(decode_ret));
                jpeg_hw_fallback_logged = true;
            }
            if (decodeFrameSw(jpeg, jpeg_size, frame) != ESP_OK) {
                return;
            }
        }
        const int64_t decode_end_us = esp_timer_get_time();
        s_stats.decode.add(decode_end_us - frame_start_us);

        // Video plays upright in the LVGL orientation: the cover crop is planned on the rotated
        // view, then the PPA turns it onto the native panel (PPA angles are counter-clockwise,
        // the LVGL rotation clockwise). The crop keeps the centre of the frame that fills the view.
        const uint16_t ui_rotation = board_display_get_rotation();
        const bool quarter_turn = (ui_rotation == 90) || (ui_rotation == 270);
        const auto panel_rotation = static_cast<ppa_srm_rotation_angle_t>((4 - ui_rotation / 90) % 4);
        const uint32_t target_w = quarter_turn ? display_h : display_w;
        const uint32_t target_h = quarter_turn ? display_w : display_h;

        // PPA truncates the scale to 1/PPA_SCALE_STEPS, so an exact target / crop ratio
        // leaves the output short of the panel and the uncovered strip shows whatever
        // each frame buffer last held. Round the smallest cover scale up to a step
        // instead, and crop the source to what that step fills.
        const uint32_t scale_steps = std::max(
            ceil_div(target_w * PPA_SCALE_STEPS, frame.width),
            ceil_div(target_h * PPA_SCALE_STEPS, frame.height)
        );
        const float scale = static_cast<float>(scale_steps) / PPA_SCALE_STEPS;
        const uint32_t crop_w = std::max<uint32_t>(target_w * PPA_SCALE_STEPS / scale_steps, 1);
        const uint32_t crop_h = std::max<uint32_t>(target_h * PPA_SCALE_STEPS / scale_steps, 1);
        const uint32_t crop_x = (frame.width - crop_w) / 2;
        const uint32_t crop_y = (frame.height - crop_h) / 2;

        // What is left uncovered is at most a pixel or two; center the picture in it.
        const uint32_t out_w = crop_w * scale_steps / PPA_SCALE_STEPS;
        const uint32_t out_h = crop_h * scale_steps / PPA_SCALE_STEPS;
        const uint32_t panel_out_w = quarter_turn ? out_h : out_w;
        const uint32_t panel_out_h = quarter_turn ? out_w : out_h;

        if (last_video_width != frame.width || last_video_height != frame.height) {
            // The PPA never writes the margins, so blank them once in every frame buffer.
            const size_t lcd_buffer_size = static_cast<size_t>(display_w) * display_h * bytes_per_pixel;
            for (int i = 0; i < display_info.frame_buffer_num; i++) {
                memset(lcd_buffers[i], 0, lcd_buffer_size);
                esp_cache_msync(lcd_buffers[i], lcd_buffer_size,
                                ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
            }

            ESP_LOGI(ESP_UTILS_LOG_TAG, "Cover video frame: %lux%lu (decoded %lux%lu), crop %lu,%lu %lux%lu",
                     static_cast<unsigned long>(frame.width), static_cast<unsigned long>(frame.height),
                     static_cast<unsigned long>(frame.pic_w), static_cast<unsigned long>(frame.pic_h),
                     static_cast<unsigned long>(crop_x), static_cast<unsigned long>(crop_y),
                     static_cast<unsigned long>(crop_w), static_cast<unsigned long>(crop_h));
            last_video_width = frame.width;
            last_video_height = frame.height;
        }

        void *lcd_buffer = lcd_buffers[next_buf_idx];

        ppa_srm_oper_config_t srm_config = {};
        srm_config.in.buffer = frame.buffer;
        srm_config.in.pic_w = frame.pic_w;
        srm_config.in.pic_h = frame.pic_h;
        srm_config.in.block_w = crop_w;
        srm_config.in.block_h = crop_h;
        srm_config.in.block_offset_x = crop_x;
        srm_config.in.block_offset_y = crop_y;
        srm_config.in.srm_cm = ppa_color_mode;

        srm_config.out.buffer = lcd_buffer;
        srm_config.out.buffer_size = ALIGN_UP(display_w * display_h * bytes_per_pixel, data_cache_line_size);
        srm_config.out.pic_w = display_w;
        srm_config.out.pic_h = display_h;
        srm_config.out.block_offset_x = (display_w - panel_out_w) / 2;
        srm_config.out.block_offset_y = (display_h - panel_out_h) / 2;
        srm_config.out.srm_cm = ppa_color_mode;

        srm_config.rotation_angle = panel_rotation;
        srm_config.scale_x = scale;
        srm_config.scale_y = scale;
        // No flip either: mirroring both axes would turn the picture upside down.
        srm_config.mirror_x = 0;
        srm_config.mirror_y = 0;
        srm_config.rgb_swap = 0;
        srm_config.byte_swap = 0;
        srm_config.mode = PPA_TRANS_MODE_BLOCKING;

        esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_srm_handle, &srm_config);
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "PPA scale failed: %s", esp_err_to_name(ret));
            return;
        }
        const int64_t ppa_end_us = esp_timer_get_time();
        s_stats.ppa.add(ppa_end_us - decode_end_us);

        ret = esp_lv_adapter_dummy_draw_blit(display, 0, 0, display_w, display_h, lcd_buffer, false);
        if (ret != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Dummy draw blit failed: %s", esp_err_to_name(ret));
            return;
        }
        current_buf_idx = next_buf_idx;
        s_stats.blit.add(esp_timer_get_time() - ppa_end_us);
        s_stats.frames++;
        playback_stats_maybe_log();
    }

    /*
     * Plays the audio queue into I2S. Each blocking write paces the whole pipeline and
     * re-anchors the master clock, so the clock follows what the speaker is playing.
     */
    void VideoPlayer::audioTask(void *arg)
    {
        Playback *pb = static_cast<Playback *>(arg);
        while (true) {
            esp_extractor_frame_info_t frame = {};
            if (xQueueReceive(pb->audio_queue, &frame, QUEUE_POLL_TICKS) != pdTRUE) {
                if (pb->stop.load()) {
                    break;
                }
                continue;
            }
            if (frame.frame_buffer == nullptr) {
                break;  // End of the audio track
            }
            if (pb->stop.load()) {
                release_frame(pb->extractor, frame);
                break;
            }

            size_t written = 0;
            const int64_t write_start_us = esp_timer_get_time();
            esp_err_t ret = board_i2s_write(frame.frame_buffer, frame.frame_size, &written, 1000);
            const int64_t write_end_us = esp_timer_get_time();
            s_stats.audio_write.add(write_end_us - write_start_us);
            s_stats.audio_chunks++;
            s_stats.audio_bytes += frame.frame_size;
            if (ret != ESP_OK || written != frame.frame_size) {
                ESP_LOGW(ESP_UTILS_LOG_TAG, "Audio write failed: %s, %u/%u bytes", esp_err_to_name(ret),
                         static_cast<unsigned>(written), static_cast<unsigned>(frame.frame_size));
            }

            // The ring drains in real time and gains each chunk; a write that blocked
            // returns only once the chunk fits, i.e. with the ring full.
            const int64_t chunk_ms = static_cast<int64_t>(frame.frame_size) * 1000 / pb->audio_bytes_per_sec;
            int64_t level = pb->dma_level_ms;
            if (pb->last_write_end_us != 0) {
                level -= (write_end_us - pb->last_write_end_us) / 1000;
            }
            level = (level > 0 ? level : 0) + chunk_ms;
            if (write_end_us - write_start_us > AUDIO_WRITE_BLOCKED_US || level > pb->audio_dma_ms) {
                level = pb->audio_dma_ms;
            }
            pb->dma_level_ms = level;
            pb->last_write_end_us = write_end_us;

            const int64_t written_end_ms = static_cast<int64_t>(frame.pts) + chunk_ms;
            pb->setClock(write_end_us, written_end_ms - level, written_end_ms);
            release_frame(pb->extractor, frame);
        }
        // Video may outlast the audio track; keep its clock running.
        pb->releaseClockLimit();
        xSemaphoreGive(pb->worker_done);
        vTaskDelete(nullptr);
    }

    /*
     * Shows each video frame at its PTS on the master clock: early frames wait, frames
     * too late are dropped before decoding so the decoder catches up.
     */
    void VideoPlayer::videoTask(void *arg)
    {
        Playback *pb = static_cast<Playback *>(arg);
        VideoPlayer *self = pb->player;
        int consecutive_drops = 0;
        int64_t first_frame_us = 0;

        while (true) {
            esp_extractor_frame_info_t frame = {};
            if (xQueueReceive(pb->video_queue, &frame, QUEUE_POLL_TICKS) != pdTRUE) {
                if (pb->stop.load()) {
                    break;
                }
                continue;
            }
            if (frame.frame_buffer == nullptr) {
                break;  // End of the video track
            }

            const int64_t pts_ms = frame.pts;
            int64_t clock_ms = 0;
            bool show = true;
            if (first_frame_us == 0) {
                first_frame_us = esp_timer_get_time();
            }
            // Without audio, or if audio never starts, run on the system clock.
            while (!pb->stop.load() && !pb->clock(clock_ms)) {
                if (!pb->has_audio || esp_timer_get_time() - first_frame_us > AUDIO_START_TIMEOUT_MS * 1000) {
                    pb->setClock(esp_timer_get_time(), pts_ms, INT64_MAX);
                    continue;
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            // Early: wait for the frame's time, re-reading the clock as audio re-anchors it.
            int64_t waited_ms = 0;
            while (!pb->stop.load() && pb->clock(clock_ms) && pts_ms > clock_ms &&
                    waited_ms < VIDEO_MAX_EARLY_WAIT_MS) {
                int64_t wait_ms = pts_ms - clock_ms;
                wait_ms = wait_ms < 20 ? wait_ms : 20;
                vTaskDelay(pdMS_TO_TICKS(wait_ms > 0 ? wait_ms : 1));
                waited_ms += wait_ms;
            }
            if (pb->stop.load()) {
                show = false;
            } else if (clock_ms - pts_ms > VIDEO_LATE_DROP_MS && consecutive_drops < VIDEO_MAX_CONSECUTIVE_DROPS) {
                show = false;
                consecutive_drops++;
                s_stats.dropped++;
            }

            if (show) {
                self->renderJpegFrame(frame.frame_buffer, frame.frame_size);
                consecutive_drops = 0;
            }
            release_frame(pb->extractor, frame);
        }
        xSemaphoreGive(pb->worker_done);
        vTaskDelete(nullptr);
    }

    bool VideoPlayer::playbackInterrupted(Playback &pb, bool &request_close)
    {
        if (!loop_playback.load() || is_paused.load()) {
            pb.stop.store(true);
            return true;
        }
        if (shouldExitBySwipe()) {
            request_close = true;
            loop_playback.store(false);
            pb.stop.store(true);
            return true;
        }
        return pb.stop.load();
    }

    /* Demuxes one file into the audio and video queues until it ends or playback stops. */
    void VideoPlayer::playFile(const char *path, bool &request_close)
    {
        Playback pb;
        pb.player = this;
        bool demux_started = false;

        do {
            FileSource &src = pb.source;
            src.file = fopen(path, "rb");
            if (!src.file) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Open %s failed", path);
                break;
            }
            // A larger, DMA-capable stdio buffer makes SD card reads much faster. SDMMC on the P4
            // can DMA into cache-line aligned PSRAM, so keep this buffer out of internal DMA RAM:
            // esp_hosted asserts when it cannot get internal DMA memory for Wi-Fi RX packets.
            src.io_buffer = heap_caps_malloc(FILE_IO_BUFFER_SIZE,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED);
            if (src.io_buffer) {
                setvbuf(src.file, static_cast<char *>(src.io_buffer), _IOFBF, FILE_IO_BUFFER_SIZE);
            }
            if (fseek(src.file, 0, SEEK_END) == 0) {
                long size = ftell(src.file);
                src.size = size > 0 ? static_cast<uint32_t>(size) : 0;
            }
            fseek(src.file, 0, SEEK_SET);

            esp_extractor_config_t config = {};
            config.type = ESP_EXTRACTOR_TYPE_AVI;
            config.extract_mask = audio_ready.load() ? ESP_EXTRACT_MASK_AV : ESP_EXTRACT_MASK_VIDEO;
            config.in_read_cb = extractor_file_read;
            config.in_seek_cb = extractor_file_seek;
            config.in_size_cb = extractor_file_size;
            config.in_ctx = &src;
            config.out_pool_size = EXTRACTOR_POOL_SIZE;
            config.out_align = EXTRACTOR_OUT_ALIGN;
            esp_extractor_err_t ret = esp_extractor_open(&config, &pb.extractor);
            if (ret != ESP_EXTRACTOR_ERR_OK) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Open extractor failed: %d", static_cast<int>(ret));
                pb.extractor = nullptr;
                break;
            }
            ret = esp_extractor_parse_stream(pb.extractor);
            if (ret != ESP_EXTRACTOR_ERR_OK) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Parse %s failed: %d", path, static_cast<int>(ret));
                break;
            }

            uint16_t video_num = 0;
            esp_extractor_stream_info_t video_info = {};
            if (esp_extractor_get_stream_num(pb.extractor, ESP_EXTRACTOR_STREAM_TYPE_VIDEO, &video_num) !=
                    ESP_EXTRACTOR_ERR_OK || video_num == 0 ||
                    esp_extractor_get_stream_info(pb.extractor, ESP_EXTRACTOR_STREAM_TYPE_VIDEO, 0, &video_info) !=
                    ESP_EXTRACTOR_ERR_OK) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "%s has no video stream", path);
                break;
            }
            if (video_info.video_info.format != ESP_EXTRACTOR_VIDEO_FORMAT_MJPEG) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "%s: only MJPEG video is supported", path);
                break;
            }
            ESP_LOGI(ESP_UTILS_LOG_TAG, "Video: MJPEG %ux%u @ %u fps, %lu ms",
                     video_info.video_info.width, video_info.video_info.height, video_info.video_info.fps,
                     static_cast<unsigned long>(video_info.duration));

            uint16_t audio_num = 0;
            esp_extractor_stream_info_t audio_info = {};
            if (audio_ready.load() &&
                    esp_extractor_get_stream_num(pb.extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, &audio_num) ==
                    ESP_EXTRACTOR_ERR_OK && audio_num > 0 &&
                    esp_extractor_get_stream_info(pb.extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, 0, &audio_info) ==
                    ESP_EXTRACTOR_ERR_OK) {
                const auto &a = audio_info.audio_info;
                ESP_LOGI(ESP_UTILS_LOG_TAG, "Audio: %lu Hz, %u bit, %u ch",
                         static_cast<unsigned long>(a.sample_rate), a.bits_per_sample, a.channel);
                if (a.format == ESP_EXTRACTOR_AUDIO_FORMAT_PCM && a.sample_rate > 0 && a.bits_per_sample > 0 &&
                        a.channel > 0) {
                    const i2s_slot_mode_t mode = (a.channel <= 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;
                    esp_err_t codec_ret = board_codec_set_fs(a.sample_rate, a.bits_per_sample, mode);
                    if (codec_ret == ESP_OK) {
                        pb.has_audio = true;
                        pb.audio_bytes_per_sec = a.sample_rate * a.channel * (a.bits_per_sample / 8);
                        pb.audio_dma_ms = static_cast<int64_t>(BOARD_I2S_DMA_DESC_NUM) *
                                          BOARD_I2S_DMA_FRAME_NUM * 1000 / a.sample_rate;
                    } else {
                        ESP_LOGW(ESP_UTILS_LOG_TAG, "Audio clock setup failed: %s", esp_err_to_name(codec_ret));
                    }
                } else {
                    ESP_LOGW(ESP_UTILS_LOG_TAG, "Only PCM audio is supported; playing video only");
                }
            }
            if (!pb.has_audio && audio_num > 0) {
                (void)esp_extractor_enable_stream(pb.extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, 0, false);
            }

            pb.video_queue = xQueueCreate(VIDEO_QUEUE_LEN, sizeof(esp_extractor_frame_info_t));
            pb.audio_queue = pb.has_audio ? xQueueCreate(AUDIO_QUEUE_LEN, sizeof(esp_extractor_frame_info_t)) : nullptr;
            pb.worker_done = xSemaphoreCreateCounting(2, 0);
            if (!pb.video_queue || (pb.has_audio && !pb.audio_queue) || !pb.worker_done) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Create playback queues failed");
                break;
            }

            playback_stats_reset();
            // Audio runs on core 1 (LVGL is paused during playback) so decoding cannot starve it.
            if (pb.has_audio) {
                if (xTaskCreatePinnedToCore(audioTask, "avi_audio", AUDIO_TASK_STACK, &pb,
                                            AUDIO_TASK_PRIORITY, nullptr, 1) != pdPASS) {
                    ESP_LOGE(ESP_UTILS_LOG_TAG, "Create audio task failed");
                    break;
                }
                pb.workers++;
            }
            if (xTaskCreatePinnedToCore(videoTask, "avi_video", VIDEO_TASK_STACK, &pb,
                                        VIDEO_TASK_PRIORITY, nullptr, 0) != pdPASS) {
                ESP_LOGE(ESP_UTILS_LOG_TAG, "Create video task failed");
                break;
            }
            pb.workers++;

            // Demux: route frames to the two queues. Blocking on a full queue paces reading.
            demux_started = true;
            int64_t next_poll_us = 0;
            bool eos = false;
            while (!eos) {
                if (esp_timer_get_time() >= next_poll_us) {
                    if (playbackInterrupted(pb, request_close)) {
                        break;
                    }
                    next_poll_us = esp_timer_get_time() + SWIPE_POLL_US;
                }
                esp_extractor_frame_info_t frame = {};
                ret = esp_extractor_read_frame(pb.extractor, &frame);
                if (ret == ESP_EXTRACTOR_ERR_WAITING_OUTPUT) {
                    vTaskDelay(pdMS_TO_TICKS(5));  // Pool full: wait for the workers to release frames
                    continue;
                }
                if (ret == ESP_EXTRACTOR_ERR_SKIPPED) {
                    ESP_LOGW(ESP_UTILS_LOG_TAG, "Frame larger than the extractor pool was skipped");
                    continue;
                }
                if (ret == ESP_EXTRACTOR_ERR_EOS) {
                    break;
                }
                if (ret != ESP_EXTRACTOR_ERR_OK) {
                    ESP_LOGE(ESP_UTILS_LOG_TAG, "Read frame failed: %d", static_cast<int>(ret));
                    break;
                }
                eos = EXTRACTOR_IS_EOS(frame.frame_flag);
                QueueHandle_t queue = (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO) ? pb.audio_queue :
                                      (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_VIDEO) ? pb.video_queue : nullptr;
                if (!queue || !frame.frame_buffer) {
                    release_frame(pb.extractor, frame);
                    continue;
                }
                bool queued = false;
                while (!queued && !pb.stop.load()) {
                    queued = xQueueSend(queue, &frame, QUEUE_POLL_TICKS) == pdTRUE;
                    if (!queued && esp_timer_get_time() >= next_poll_us) {
                        (void)playbackInterrupted(pb, request_close);
                        next_poll_us = esp_timer_get_time() + SWIPE_POLL_US;
                    }
                }
                if (!queued) {
                    release_frame(pb.extractor, frame);
                    break;
                }
            }

            // Tell the workers no more frames follow, then let them drain their queues.
            const esp_extractor_frame_info_t end_marker = {};
            QueueHandle_t queues[] = {pb.audio_queue, pb.video_queue};
            for (QueueHandle_t queue : queues) {
                while (queue && !pb.stop.load() && xQueueSend(queue, &end_marker, QUEUE_POLL_TICKS) != pdTRUE) {
                    (void)playbackInterrupted(pb, request_close);
                }
            }
        } while (false);

        // Workers exit on the end marker or on stop; keep honouring swipe and pause meanwhile.
        // Without a started demux no end marker was sent, so the workers need the stop flag.
        if (!demux_started) {
            pb.stop.store(true);
        }
        for (int i = 0; i < pb.workers; i++) {
            while (xSemaphoreTake(pb.worker_done, QUEUE_POLL_TICKS) != pdTRUE) {
                (void)playbackInterrupted(pb, request_close);
            }
        }
        // Every frame must go back to the pool before the extractor can close.
        QueueHandle_t queues[] = {pb.audio_queue, pb.video_queue};
        for (QueueHandle_t queue : queues) {
            esp_extractor_frame_info_t frame = {};
            while (queue && xQueueReceive(queue, &frame, 0) == pdTRUE) {
                release_frame(pb.extractor, frame);
            }
            if (queue) {
                vQueueDelete(queue);
            }
        }
        if (pb.worker_done) {
            vSemaphoreDelete(pb.worker_done);
        }
        if (pb.extractor) {
            esp_extractor_close(pb.extractor);
        }
        if (pb.source.file) {
            fclose(pb.source.file);
        }
        heap_caps_free(pb.source.io_buffer);
    }

    bool VideoPlayer::shouldExitBySwipe()
    {
        if (!touch_handle) {
            return false;
        }

        esp_lcd_touch_read_data(touch_handle);

        esp_lcd_touch_point_data_t points[1] = {};
        uint8_t point_num = 0;
        esp_err_t ret = esp_lcd_touch_get_data(touch_handle, points, &point_num, 1);

        if (ret != ESP_OK || point_num == 0) {
            touch_active = false;
            return false;
        }

        if (!touch_active) {
            touch_active = true;
            touch_start_x = points[0].x;
            touch_start_y = points[0].y;
            return false;
        }

        int dx = abs((int)points[0].x - (int)touch_start_x);
        int dy = abs((int)points[0].y - (int)touch_start_y);
        return dx > SWIPE_EXIT_THRESHOLD || dy > SWIPE_EXIT_THRESHOLD;
    }

    void VideoPlayer::playTask(void *arg)
    {
        VideoPlayer *self = static_cast<VideoPlayer *>(arg);
        bool request_close = false;
        static bool extractors_registered = false;

        self->audio_ready.store(board_codec_init() == ESP_OK);
        if (self->audio_ready.load()) {
            // Re-apply the user's current volume after (re)initializing the codec.
            esp_err_t volume_ret = board_codec_volume_set(board_codec_volume_get(), nullptr);
            if (volume_ret != ESP_OK) {
                ESP_LOGW(ESP_UTILS_LOG_TAG, "Set video volume failed: %s", esp_err_to_name(volume_ret));
            }
        } else {
            ESP_LOGW(ESP_UTILS_LOG_TAG, "Audio codec unavailable; continue video-only playback");
        }

        if (!extractors_registered) {
            extractors_registered = esp_extractor_register_default() == ESP_EXTRACTOR_ERR_OK;
        }
        if (!extractors_registered) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Register extractors failed");
        } else if (self->loop_playback.load() && self->initDisplayBypass() != ESP_OK) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "initDisplayBypass failed");
        } else {
            // Loop through every AVI file discovered on the SD card until the app closes.
            while (self->loop_playback.load()) {
                for (int i = 0; i < self->avi_file_count && self->loop_playback.load(); i++) {
                    while (self->is_paused.load() && self->loop_playback.load()) {
                        if (self->shouldExitBySwipe()) {
                            request_close = true;
                            self->loop_playback.store(false);
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    if (!self->loop_playback.load()) {
                        break;
                    }
                    ESP_LOGI(ESP_UTILS_LOG_TAG, "Playing AVI file: %s", self->avi_file_list[i]);
                    self->playFile(self->avi_file_list[i], request_close);
                }
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }

        self->audio_ready.store(false);

        /* initDisplayBypass pauses LVGL. It must be unwound before publishing
         * a close request, otherwise the GUI timer that consumes that request
         * can never run. playFile() has joined its workers, so no direct draw
         * can race this handoff. */
        self->deinitDisplayBypass();

        if (request_close)
        {
            // Worker core 0 only publishes the request. The GUI-owned LVGL
            // timer performs notifyCoreClosed() after worker ownership ends.
            self->close_requested.store(true);
        }

        self->loop_playback.store(false);
        self->playback_exited.store(true);

        vTaskDeleteWithCaps(NULL);
    }

    void VideoPlayer::closeRequestTimerCallback(lv_timer_t *timer)
    {
        auto *self = static_cast<VideoPlayer *>(lv_timer_get_user_data(timer));
        if (!self || !self->close_requested.load() || !self->playback_exited.load()) {
            return;
        }

        bool expected = false;
        if (!self->close_notification_posted.compare_exchange_strong(expected, true)) {
            return;
        }
        if (!self->notifyCoreClosed()) {
            ESP_LOGE(ESP_UTILS_LOG_TAG, "Notify video close failed");
            self->close_notification_posted.store(false);
        }
    }

    // The app is a singleton owned by itself; the registry must not delete it.
    ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, VideoPlayer, "VideoPlayer", []()
    {
        return std::shared_ptr<VideoPlayer>(VideoPlayer::requestInstance(), [](VideoPlayer *) {});
    })

} // namespace esp_brookesia::apps
