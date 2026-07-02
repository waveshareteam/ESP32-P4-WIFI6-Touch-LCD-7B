/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"

static const char *TAG = "bsp_extra_audio";

static esp_codec_dev_handle_t s_play_dev;
static esp_codec_dev_handle_t s_record_dev;
static bool s_audio_initialized;
static bool s_play_opened;
static bool s_record_opened;
static int s_volume = CODEC_DEFAULT_VOLUME;

esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    ESP_RETURN_ON_FALSE(s_play_dev || s_record_dev, ESP_ERR_INVALID_STATE, TAG, "codec is not initialized");

    esp_err_t ret = ESP_OK;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (s_play_opened) {
        ret |= esp_codec_dev_close(s_play_dev);
        s_play_opened = false;
    }
    if (s_record_opened) {
        ret |= esp_codec_dev_close(s_record_dev);
        s_record_opened = false;
    }

    if (s_play_dev) {
        ret |= esp_codec_dev_open(s_play_dev, &fs);
        if (ret == ESP_OK) {
            s_play_opened = true;
        }
    }
    if (s_record_dev) {
        ret |= esp_codec_dev_set_in_gain(s_record_dev, 24.0);
        ret |= esp_codec_dev_open(s_record_dev, &fs);
        if (ret == ESP_OK) {
            s_record_opened = true;
        }
    }

    return ret;
}

esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_FALSE(s_play_dev, ESP_ERR_INVALID_STATE, TAG, "speaker codec is not initialized");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(s_play_dev, volume), TAG, "set codec volume failed");

    s_volume = volume;
    if (volume_set) {
        *volume_set = s_volume;
    }
    return ESP_OK;
}

esp_err_t bsp_extra_codec_init(void)
{
    if (s_audio_initialized) {
        return ESP_OK;
    }

    s_play_dev = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(s_play_dev, ESP_FAIL, TAG, "speaker codec init failed");

    s_record_dev = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(s_record_dev, ESP_FAIL, TAG, "microphone codec init failed");

    ESP_RETURN_ON_ERROR(bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL), TAG, "set codec format failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL), TAG, "set codec volume failed");

    s_audio_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)timeout_ms;
    ESP_RETURN_ON_FALSE(s_record_dev, ESP_ERR_INVALID_STATE, TAG, "microphone codec is not initialized");

    esp_err_t ret = esp_codec_dev_read(s_record_dev, audio_buffer, len);
    if (bytes_read) {
        *bytes_read = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}

esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    (void)timeout_ms;
    ESP_RETURN_ON_FALSE(s_play_dev, ESP_ERR_INVALID_STATE, TAG, "speaker codec is not initialized");

    esp_err_t ret = esp_codec_dev_write(s_play_dev, audio_buffer, len);
    if (bytes_written) {
        *bytes_written = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}
