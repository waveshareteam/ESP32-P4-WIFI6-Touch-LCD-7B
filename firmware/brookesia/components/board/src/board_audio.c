/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "board_audio.h"
#include "board_audio_internal.h"

static const char *TAG = "board_audio";
static bool _is_player_init = false;

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
        return ESP_ERR_INVALID_SIZE;
    }
    if (length != NULL) {
        *length = path_length;
    }
    return ESP_OK;
}

static esp_err_t set_audio_file_path(const char *file_path)
{
    size_t path_length;
    ESP_RETURN_ON_ERROR(audio_file_path_length(file_path, &path_length), TAG, "Invalid audio file path");
    memcpy(audio_file_path, file_path, path_length + 1);
    return ESP_OK;
}

static void audio_callback(audio_player_cb_ctx_t *ctx)
{
    if (audio_idle_callback) {
        ctx->user_ctx = audio_idle_cb_user_data;
        audio_idle_callback(ctx);
    }
}

esp_err_t board_player_init(void)
{
    if (_is_player_init) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(board_codec_init(), TAG, "Initialize codec failed");
    ESP_RETURN_ON_ERROR(board_codec_dev_resume(), TAG, "Resume output codec failed");
    audio_player_config_t config = {
        .mute_fn = board_audio_player_mute,
        .write_fn = board_i2s_write,
        .clk_set_fn = board_codec_set_fs,
        .priority = 5
    };
    ESP_RETURN_ON_ERROR(audio_player_new(config), TAG, "audio_player_init failed");
    audio_player_callback_register(audio_callback, NULL);

    _is_player_init = true;

    return ESP_OK;
}

esp_err_t board_player_del(void)
{
    esp_err_t ret = audio_player_delete();
    ESP_RETURN_ON_ERROR(ret, TAG, "Delete audio player failed");
    _is_player_init = false;
    ESP_RETURN_ON_ERROR(board_codec_output_stop(), TAG, "Stop playback codec failed");

    return ESP_OK;
}

esp_err_t board_file_instance_init(const char *path, file_iterator_instance_t **ret_instance)
{
    ESP_RETURN_ON_FALSE(path, ESP_FAIL, TAG, "path is NULL");
    ESP_RETURN_ON_FALSE(ret_instance, ESP_FAIL, TAG, "ret_instance is NULL");

    file_iterator_instance_t *file_iterator = file_iterator_new(path);
    ESP_RETURN_ON_FALSE(file_iterator, ESP_FAIL, TAG, "file_iterator_new failed, %s", path);

    *ret_instance = file_iterator;

    return ESP_OK;
}

esp_err_t board_player_play_index(file_iterator_instance_t *instance, int index)
{
    ESP_RETURN_ON_FALSE(instance, ESP_FAIL, TAG, "instance is NULL");

    ESP_LOGI(TAG, "play_index(%d)", index);
    char filename[128];
    int retval = file_iterator_get_full_path_from_index(instance, index, filename, sizeof(filename));
    ESP_RETURN_ON_FALSE(retval != 0, ESP_FAIL, TAG, "file_iterator_get_full_path_from_index failed");
    ESP_RETURN_ON_ERROR(audio_file_path_length(filename, NULL), TAG, "Invalid audio file path");

    ESP_LOGI(TAG, "opening file '%s'", filename);
    FILE *fp = fopen(filename, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", filename);
    esp_err_t ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        fclose(fp);
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
        return ret;
    }

    file_iterator_set_index(instance, index);
    ESP_RETURN_ON_ERROR(set_audio_file_path(filename), TAG, "Invalid audio file path");

    return ESP_OK;
}

esp_err_t board_player_play_file(const char *file_path)
{
    ESP_RETURN_ON_ERROR(audio_file_path_length(file_path, NULL), TAG, "Invalid audio file path");
    ESP_LOGI(TAG, "opening file '%s'", file_path);
    FILE *fp = fopen(file_path, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", file_path);
    esp_err_t ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        fclose(fp);
        ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_RETURN_ON_ERROR(set_audio_file_path(file_path), TAG, "Invalid audio file path");

    return ESP_OK;
}

void board_player_register_callback(audio_player_cb_t cb, void *user_data)
{
    audio_idle_callback = cb;
    audio_idle_cb_user_data = user_data;
}

bool board_player_is_playing_by_path(const char *file_path)
{
    if (audio_file_path_length(file_path, NULL) != ESP_OK) {
        return false;
    }
    return (strcmp(audio_file_path, file_path) == 0);
}

bool board_player_is_playing_by_index(file_iterator_instance_t *instance, int index)
{
    return (index == file_iterator_get_index(instance));
}
