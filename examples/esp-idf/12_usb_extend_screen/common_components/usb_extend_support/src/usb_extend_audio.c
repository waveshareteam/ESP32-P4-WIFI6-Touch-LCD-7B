/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "esp_check.h"

#include "bsp/esp-bsp.h"
#include "usb_extend_support/audio.h"

static const char *TAG = "usb_extend_audio_board";

static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;

static bool _is_audio_init = false;
static bool _is_player_init = false;
static int _vloume_intensity = CODEC_DEFAULT_VOLUME;

static audio_player_cb_t audio_idle_callback = NULL;
static void *audio_idle_cb_user_data = NULL;
static char audio_file_path[128];

static esp_err_t audio_file_path_length(const char *file_path, size_t *length)
{
    if (file_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t path_length = strnlen(file_path, sizeof(audio_file_path));
    if (path_length >= sizeof(audio_file_path)) {
        ESP_LOGE(TAG, "Audio file path is too long");
        return ESP_ERR_INVALID_SIZE;
    }

    if (length != NULL) {
        *length = path_length;
    }
    return ESP_OK;
}

static esp_err_t set_audio_file_path(const char *file_path)
{
    size_t path_length = 0;
    esp_err_t ret = audio_file_path_length(file_path, &path_length);
    if (ret != ESP_OK) {
        return ret;
    }

    memcpy(audio_file_path, file_path, path_length + 1);
    return ESP_OK;
}

static esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    // Volume saved when muting and restored when unmuting. Restoring volume is necessary
    // as es8311_set_voice_mute(true) results in voice volume (REG32) being set to zero.

    usb_extend_audio_codec_mute_set(setting == AUDIO_PLAYER_MUTE ? true : false);

    // restore the voice volume upon unmuting
    if (setting == AUDIO_PLAYER_UNMUTE) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, _vloume_intensity), TAG, "Set Codec volume failed");
    }

    return ESP_OK;
}

static void audio_callback(audio_player_cb_ctx_t *ctx)
{
    if (audio_idle_callback) {
        ctx->user_ctx = audio_idle_cb_user_data;
        audio_idle_callback(ctx);
    }
}

esp_err_t usb_extend_audio_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_read(record_dev_handle, audio_buffer, len);
    if (bytes_read != NULL) {
        *bytes_read = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}

esp_err_t usb_extend_audio_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    if (bytes_written != NULL) {
        *bytes_written = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}

esp_err_t usb_extend_audio_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (play_dev_handle) {
        ret = esp_codec_dev_close(play_dev_handle);
    }
    if (record_dev_handle) {
        ret |= esp_codec_dev_close(record_dev_handle);
        ret |= esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);
    }

    if (play_dev_handle) {
        ret |= esp_codec_dev_open(play_dev_handle, &fs);
    }
    if (record_dev_handle) {
        ret |= esp_codec_dev_open(record_dev_handle, &fs);
    }
    return ret;
}

esp_err_t usb_extend_audio_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, volume), TAG, "Set Codec volume failed");
    _vloume_intensity = volume;

    ESP_LOGI(TAG, "Setting volume: %d", volume);

    return ESP_OK;
}

int usb_extend_audio_codec_volume_get(void)
{
    return _vloume_intensity;
}

esp_err_t usb_extend_audio_codec_mute_set(bool enable)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_set_out_mute(play_dev_handle, enable);
    return ret;
}

esp_err_t usb_extend_audio_codec_dev_stop(void)
{
    esp_err_t ret = ESP_OK;

    if (play_dev_handle) {
        ret = esp_codec_dev_close(play_dev_handle);
    }

    if (record_dev_handle) {
        ret = esp_codec_dev_close(record_dev_handle);
    }
    return ret;
}

esp_err_t usb_extend_audio_codec_dev_resume(void)
{
    return usb_extend_audio_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
}

esp_err_t usb_extend_audio_codec_init()
{
    if (_is_audio_init) {
        return ESP_OK;
    }

    play_dev_handle = bsp_audio_codec_speaker_init();
    assert((play_dev_handle) && "play_dev_handle not initialized");

    record_dev_handle = bsp_audio_codec_microphone_init();
    assert((record_dev_handle) && "record_dev_handle not initialized");

    usb_extend_audio_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);

    _is_audio_init = true;

    return ESP_OK;
}

esp_err_t usb_extend_audio_player_init(void)
{
    if (_is_player_init) {
        return ESP_OK;
    }

    audio_player_config_t config = { .mute_fn = audio_mute_function,
                                     .write_fn = usb_extend_audio_i2s_write,
                                     .clk_set_fn = usb_extend_audio_codec_set_fs,
                                     .priority = 5
                                   };
    ESP_RETURN_ON_ERROR(audio_player_new(config), TAG, "audio_player_init failed");
    audio_player_callback_register(audio_callback, NULL);

    _is_player_init = true;

    return ESP_OK;
}

esp_err_t usb_extend_audio_player_del(void)
{
    _is_player_init = false;

    ESP_RETURN_ON_ERROR(audio_player_delete(), TAG, "audio_player_delete failed");

    return ESP_OK;
}

esp_err_t usb_extend_audio_file_instance_init(const char *path, file_iterator_instance_t **ret_instance)
{
    ESP_RETURN_ON_FALSE(path, ESP_FAIL, TAG, "path is NULL");
    ESP_RETURN_ON_FALSE(ret_instance, ESP_FAIL, TAG, "ret_instance is NULL");

    file_iterator_instance_t *file_iterator = file_iterator_new(path);
    ESP_RETURN_ON_FALSE(file_iterator, ESP_FAIL, TAG, "file_iterator_new failed, %s", path);

    *ret_instance = file_iterator;

    return ESP_OK;
}

esp_err_t usb_extend_audio_player_play_index(file_iterator_instance_t *instance, int index)
{
    ESP_RETURN_ON_FALSE(instance, ESP_FAIL, TAG, "instance is NULL");

    ESP_LOGI(TAG, "play_index(%d)", index);
    char filename[128] = {0};
    int retval = file_iterator_get_full_path_from_index(instance, index, filename, sizeof(filename));
    ESP_RETURN_ON_FALSE(retval != 0, ESP_FAIL, TAG, "file_iterator_get_full_path_from_index failed");
    ESP_RETURN_ON_ERROR(audio_file_path_length(filename, NULL), TAG, "invalid audio file path");

    ESP_LOGI(TAG, "opening file '%s'", filename);
    FILE *fp = fopen(filename, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", filename);
    ESP_RETURN_ON_ERROR(audio_player_play(fp), TAG, "audio_player_play failed");
    ESP_RETURN_ON_ERROR(set_audio_file_path(filename), TAG, "invalid audio file path");

    return ESP_OK;
}

esp_err_t usb_extend_audio_player_play_file(const char *file_path)
{
    ESP_RETURN_ON_ERROR(audio_file_path_length(file_path, NULL), TAG, "invalid audio file path");
    ESP_LOGI(TAG, "opening file '%s'", file_path);
    FILE *fp = fopen(file_path, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", file_path);
    ESP_RETURN_ON_ERROR(audio_player_play(fp), TAG, "audio_player_play failed");

    ESP_RETURN_ON_ERROR(set_audio_file_path(file_path), TAG, "invalid audio file path");

    return ESP_OK;
}

void usb_extend_audio_player_register_callback(audio_player_cb_t cb, void *user_data)
{
    audio_idle_callback = cb;
    audio_idle_cb_user_data = user_data;
}

bool usb_extend_audio_player_is_playing_by_path(const char *file_path)
{
    if (audio_file_path_length(file_path, NULL) != ESP_OK) {
        return false;
    }
    return (strcmp(audio_file_path, file_path) == 0);
}

bool usb_extend_audio_player_is_playing_by_index(file_iterator_instance_t *instance, int index)
{
    return (index == file_iterator_get_index(instance));
}
