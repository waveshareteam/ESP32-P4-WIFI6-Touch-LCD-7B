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
#include "esp_attr.h"
#include "esp_err.h"
#include "driver/i2s_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_board_manager_includes.h"

#include "board_audio.h"
#include "board_audio_internal.h"

static const char *TAG = "board_codec";

/* Owned by the esp_board_manager audio_dac (ES8311) and audio_adc (ES7210) devices. */
static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;
static volatile uint32_t i2s_tx_underrun_count;

static bool _is_audio_init = false;
static int _vloume_intensity = CODEC_DEFAULT_VOLUME;
/* Last output format opened on the codec, so dev_resume() restores the track's
 * sample rate instead of falling back to the default one. */
static uint32_t _out_rate = CODEC_DEFAULT_SAMPLE_RATE;
static uint32_t _out_bits = CODEC_DEFAULT_BIT_WIDTH;
static i2s_slot_mode_t _out_ch = CODEC_DEFAULT_CHANNEL;
static SemaphoreHandle_t audio_mutex;
static SemaphoreHandle_t audio_out_mutex;
static SemaphoreHandle_t audio_in_mutex;

static esp_err_t board_audio_lock(void);
static void board_audio_unlock(void);
static esp_err_t board_audio_output_lock(void);
static void board_audio_output_unlock(void);

/**************************************************************************************************
 *
 * Extra Board Function
 *
 **************************************************************************************************/
esp_err_t board_audio_player_mute(AUDIO_PLAYER_MUTE_SETTING setting)
{
    // Volume saved when muting and restored when unmuting. Restoring volume is necessary
    // as es8311_set_voice_mute(true) results in voice volume (REG32) being set to zero.

    ESP_RETURN_ON_ERROR(board_audio_output_lock(), TAG, "Lock player mute failed");
    esp_err_t ret = board_codec_mute_set(setting == AUDIO_PLAYER_MUTE ? true : false);

    // restore the voice volume upon unmuting
    if (ret == ESP_OK && setting == AUDIO_PLAYER_UNMUTE) {
        ret = esp_codec_dev_set_out_vol(play_dev_handle, _vloume_intensity);
    }
    board_audio_output_unlock();

    return ret;
}

/*
 * Lock order: audio_mutex (configuration) -> audio_out_mutex -> audio_in_mutex.
 * Blocking reads and writes take only their own direction lock, so a voice
 * capture loop cannot starve concurrent playback on the full-duplex bus.
 */
static esp_err_t board_audio_mutex_init(void)
{
    if (!audio_mutex) {
        audio_mutex = xSemaphoreCreateRecursiveMutex();
    }
    if (!audio_out_mutex) {
        audio_out_mutex = xSemaphoreCreateRecursiveMutex();
    }
    if (!audio_in_mutex) {
        audio_in_mutex = xSemaphoreCreateRecursiveMutex();
    }
    return audio_mutex && audio_out_mutex && audio_in_mutex ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t board_audio_take(SemaphoreHandle_t mutex)
{
    return xSemaphoreTakeRecursive(mutex, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

/* Reconfigures both directions. */
static esp_err_t board_audio_lock(void)
{
    ESP_RETURN_ON_ERROR(board_audio_mutex_init(), TAG, "Create audio mutex failed");
    ESP_RETURN_ON_ERROR(board_audio_take(audio_mutex), TAG, "Lock audio config failed");
    if (board_audio_take(audio_out_mutex) != ESP_OK) {
        xSemaphoreGiveRecursive(audio_mutex);
        return ESP_ERR_TIMEOUT;
    }
    if (board_audio_take(audio_in_mutex) != ESP_OK) {
        xSemaphoreGiveRecursive(audio_out_mutex);
        xSemaphoreGiveRecursive(audio_mutex);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void board_audio_unlock(void)
{
    xSemaphoreGiveRecursive(audio_in_mutex);
    xSemaphoreGiveRecursive(audio_out_mutex);
    xSemaphoreGiveRecursive(audio_mutex);
}

/* Reconfigures the playback side only; capture keeps running. */
static esp_err_t board_audio_output_lock(void)
{
    ESP_RETURN_ON_ERROR(board_audio_mutex_init(), TAG, "Create audio mutex failed");
    ESP_RETURN_ON_ERROR(board_audio_take(audio_mutex), TAG, "Lock audio config failed");
    if (board_audio_take(audio_out_mutex) != ESP_OK) {
        xSemaphoreGiveRecursive(audio_mutex);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void board_audio_output_unlock(void)
{
    xSemaphoreGiveRecursive(audio_out_mutex);
    xSemaphoreGiveRecursive(audio_mutex);
}

/* Data path locks: exclude reconfiguration of the same direction only. */
static esp_err_t board_audio_write_lock(void)
{
    ESP_RETURN_ON_ERROR(board_audio_mutex_init(), TAG, "Create audio mutex failed");
    return board_audio_take(audio_out_mutex);
}

static void board_audio_write_unlock(void)
{
    xSemaphoreGiveRecursive(audio_out_mutex);
}

static esp_err_t board_audio_read_lock(void)
{
    ESP_RETURN_ON_ERROR(board_audio_mutex_init(), TAG, "Create audio mutex failed");
    return board_audio_take(audio_in_mutex);
}

static void board_audio_read_unlock(void)
{
    xSemaphoreGiveRecursive(audio_in_mutex);
}

static void update_codec_result(esp_err_t *result, esp_err_t codec_result)
{
    if (*result == ESP_OK && codec_result != ESP_OK) {
        *result = codec_result;
    }
}

static esp_err_t board_audio_deinit(void)
{
    esp_err_t ret = ESP_OK;

    // Deleting the codec devices closes them; the board manager also releases I2S, I2C and the PA GPIO.
    play_dev_handle = NULL;
    record_dev_handle = NULL;
    update_codec_result(&ret, esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC));
    update_codec_result(&ret, esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC));
    _is_audio_init = false;
    return ret;
}

static bool IRAM_ATTR i2s_tx_underrun_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    i2s_tx_underrun_count++;
    return false;
}

uint32_t board_i2s_get_tx_underrun_count(void)
{
    return i2s_tx_underrun_count;
}

static esp_err_t board_i2s_tx_underrun_cb_register(void)
{
    i2s_chan_handle_t tx_chan = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_periph_handle(ESP_BOARD_PERIPH_NAME_I2S_AUDIO_OUT, (void **)&tx_chan), TAG, "Get I2S TX handle failed");
    ESP_RETURN_ON_FALSE(tx_chan, ESP_ERR_INVALID_STATE, TAG, "I2S TX handle is NULL");

    // The board manager enables the channel when it creates it, and the I2S driver only accepts
    // event callbacks on a stopped channel. No codec stream is open yet, so a short stop is harmless.
    esp_err_t ret = i2s_channel_disable(tx_chan);
    ESP_RETURN_ON_FALSE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE, ret, TAG, "Stop I2S TX failed");
    const i2s_event_callbacks_t tx_callbacks = {
        .on_send_q_ovf = i2s_tx_underrun_cb,
    };
    ret = i2s_channel_register_event_callback(tx_chan, &tx_callbacks, NULL);
    esp_err_t enable_ret = i2s_channel_enable(tx_chan);
    ESP_RETURN_ON_ERROR(ret, TAG, "Register I2S TX callback failed");
    return enable_ret;
}

static esp_err_t board_audio_devices_init(void)
{
    dev_audio_codec_handles_t *dac = NULL;
    dev_audio_codec_handles_t *adc = NULL;
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC), fail, TAG, "Initialize audio_dac failed");
    ESP_GOTO_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC), fail, TAG, "Initialize audio_adc failed");
    ESP_GOTO_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&dac), fail, TAG, "Get audio_dac handle failed");
    ESP_GOTO_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&adc), fail, TAG, "Get audio_adc handle failed");
    ESP_GOTO_ON_FALSE(dac && dac->codec_dev && adc && adc->codec_dev, ESP_ERR_INVALID_STATE, fail, TAG, "Codec device handle is NULL");
    play_dev_handle = dac->codec_dev;
    record_dev_handle = adc->codec_dev;

    // The underrun counter is diagnostics only; playback works without it.
    if (board_i2s_tx_underrun_cb_register() != ESP_OK) {
        ESP_LOGW(TAG, "I2S TX underrun counter unavailable");
    }
    return ESP_OK;

fail:
    if (board_audio_deinit() != ESP_OK) {
        ESP_LOGE(TAG, "Clean up audio devices failed");
    }
    return ret;
}

esp_err_t board_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (bytes_read) {
        *bytes_read = 0;
    }
    ESP_RETURN_ON_FALSE(record_dev_handle && audio_buffer, ESP_ERR_INVALID_STATE, TAG, "Recorder is not initialized");

    ESP_RETURN_ON_ERROR(board_audio_read_lock(), TAG, "Lock audio input failed");
    esp_err_t ret = esp_codec_dev_read(record_dev_handle, audio_buffer, len);
    if (ret == ESP_OK && bytes_read) {
        *bytes_read = len;
    }
    board_audio_read_unlock();
    return ret;
}

esp_err_t board_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (bytes_written) {
        *bytes_written = 0;
    }
    ESP_RETURN_ON_FALSE(play_dev_handle && audio_buffer, ESP_ERR_INVALID_STATE, TAG, "Player is not initialized");

    ESP_RETURN_ON_ERROR(board_audio_write_lock(), TAG, "Lock audio output failed");
    esp_err_t ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    if (ret == ESP_OK && bytes_written) {
        *bytes_written = len;
    }
    board_audio_write_unlock();
    return ret;
}

esp_err_t board_codec_output_stop(void)
{
    if (!play_dev_handle) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(board_audio_output_lock(), TAG, "Lock playback codec failed");
    esp_err_t ret = esp_codec_dev_close(play_dev_handle);
    board_audio_output_unlock();
    return ret == ESP_CODEC_DEV_OK ? ESP_OK : ret;
}

/*
 * The ES8311 driver looks up its clock coefficients by MCLK = rate * 256.
 * 24-bit slots make esp_codec_dev switch the I2S MCLK to 384 * fs, which no
 * longer matches that divider, so only 16/32-bit is allowed.
 */
static bool codec_fs_supported(uint32_t rate, uint32_t bits_cfg)
{
    if (bits_cfg != 16 && bits_cfg != 32) {
        return false;
    }
    switch (rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        return true;
    default:
        return false;
    }
}

esp_err_t board_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    ESP_RETURN_ON_FALSE(play_dev_handle && record_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Codec is not initialized");
    ESP_RETURN_ON_FALSE(codec_fs_supported(rate, bits_cfg), ESP_ERR_NOT_SUPPORTED, TAG, "ES8311 cannot clock %lu Hz / %lu-bit", (unsigned long)rate, (unsigned long)bits_cfg);
    ESP_RETURN_ON_ERROR(board_audio_output_lock(), TAG, "Lock playback reconfiguration failed");
    // Ordinary media/output reconfiguration must not close the ES7210 voice
    // capture session. Voice mode uses board_codec_set_voice_fs(), which
    // intentionally owns both sides of the shared bus.
    esp_err_t ret = board_codec_output_stop();
    if (ret != ESP_OK) {
        board_audio_output_unlock();
        ESP_LOGE(TAG, "Close playback codec failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_codec_dev_sample_info_t output_fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    ret = esp_codec_dev_open(play_dev_handle, &output_fs);
    if (ret == ESP_OK) {
        ret = esp_codec_dev_set_out_mute(play_dev_handle, false);
    }
    if (ret == ESP_OK) {
        _out_rate = rate;
        _out_bits = bits_cfg;
        _out_ch = ch;
    }
    if (ret != ESP_OK) {
        (void)esp_codec_dev_close(play_dev_handle);
    }
    board_audio_output_unlock();
    return ret;
}

esp_err_t board_audio_get_mic_layout(board_mic_layout_t *layout)
{
    // ES7210 TDM frames interleave odd inputs ahead of even ones: [MIC1, MIC3, MIC2, MIC4].
    static const uint8_t es7210_frame_index[BOARD_MIC_MAX_NUM] = {0, 2, 1, 3};
    static const char *const mic_tokens[] = {"FL", "FR", "FC", "SL", "SR", "BL", "BR"};

    ESP_RETURN_ON_FALSE(layout, ESP_ERR_INVALID_ARG, TAG, "Invalid layout pointer");
    *layout = (board_mic_layout_t) {
        .echo_ref_channel_index = -1,
    };

    dev_audio_codec_config_t *adc_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&adc_cfg), TAG,
                        "Get audio_adc config failed");
    const char *label = adc_cfg ? adc_cfg->codec_adc_cfg.label : NULL;
    ESP_RETURN_ON_FALSE(label && label[0] != '\0', ESP_ERR_NOT_FOUND, TAG, "audio_adc has no adc_cfg.label");

    // Walk the comma-separated tokens; token i describes physical input MIC(i + 1).
    int input = 0;
    for (const char *token = label; ; ++input) {
        const char *end = strchr(token, ',');
        const size_t len = end ? (size_t)(end - token) : strlen(token);
        ESP_RETURN_ON_FALSE(input < BOARD_MIC_MAX_NUM, ESP_ERR_INVALID_ARG, TAG,
                            "adc_cfg.label \"%s\" has more than %d inputs", label, BOARD_MIC_MAX_NUM);

        bool is_mic = false;
        for (size_t i = 0; i < sizeof(mic_tokens) / sizeof(mic_tokens[0]) && !is_mic; i++) {
            is_mic = len == 2 && strncmp(token, mic_tokens[i], 2) == 0;
        }
        if (is_mic) {
            const uint8_t mic = layout->mic_num++;
            layout->mic_channel_index[mic] = es7210_frame_index[input];
            memcpy(layout->mic_label[mic], token, 2);
            layout->mic_mask |= 1U << input;
        } else if (len == 2 && strncmp(token, "RE", 2) == 0) {
            layout->echo_ref_channel_index = (int8_t)es7210_frame_index[input];
            layout->echo_ref_mask |= 1U << input;
        } else {
            ESP_RETURN_ON_FALSE(len == 2 && strncmp(token, "NA", 2) == 0, ESP_ERR_INVALID_ARG, TAG,
                                "Unknown token in adc_cfg.label \"%s\"", label);
        }

        if (!end) {
            break;
        }
        token = end + 1;
    }
    return ESP_OK;
}

esp_err_t board_codec_set_voice_fs(uint32_t rate, uint32_t bits_cfg)
{
    // Capture whole TDM frames so slot filtering cannot change the DMA layout;
    // callers extract the microphones they need in software.
    const uint8_t record_channels = CODEC_VOICE_INPUT_CHANNELS;
    const uint16_t record_tdm_slot_mask = BOARD_ES7210_TDM_ALL_SLOTS_MASK;
    board_mic_layout_t mic_layout = {};
    ESP_RETURN_ON_ERROR(board_audio_get_mic_layout(&mic_layout), TAG, "Get microphone layout failed");

    ESP_RETURN_ON_FALSE(play_dev_handle && record_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Codec is not initialized");
    ESP_RETURN_ON_FALSE(codec_fs_supported(rate, bits_cfg), ESP_ERR_NOT_SUPPORTED, TAG, "ES8311 cannot clock %lu Hz / %lu-bit", (unsigned long)rate, (unsigned long)bits_cfg);
    ESP_RETURN_ON_ERROR(board_audio_lock(), TAG, "Lock voice codec reconfiguration failed");
    esp_err_t ret = board_codec_dev_stop();
    if (ret != ESP_OK) {
        board_audio_unlock();
        ESP_LOGE(TAG, "Close codec devices failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_codec_dev_sample_info_t output_fs = {
        .sample_rate = rate,
        .channel = I2S_SLOT_MODE_STEREO,
        .bits_per_sample = bits_cfg,
    };
    esp_codec_dev_sample_info_t input_fs = {
        .sample_rate = rate,
        .channel = record_channels,
        .channel_mask = record_tdm_slot_mask,
        .bits_per_sample = bits_cfg,
    };

    ret = esp_codec_dev_open(play_dev_handle, &output_fs);
    if (ret == ESP_OK) {
        ret = esp_codec_dev_set_out_mute(play_dev_handle, false);
    }
    if (ret != ESP_OK) {
        (void)board_codec_dev_stop();
        board_audio_unlock();
        return ret;
    }

    ret = esp_codec_dev_open(record_dev_handle, &input_fs);
    if (ret == ESP_OK && mic_layout.mic_mask) {
        ret = esp_codec_dev_set_in_channel_gain(record_dev_handle, mic_layout.mic_mask, CODEC_DEFAULT_ADC_VOLUME);
    }
    if (ret == ESP_OK && mic_layout.echo_ref_mask) {
        // The reference input carries the ES8311 output at line level; mic gain would clip it
        // and leave the AEC with a reference that no longer matches the echo.
        ret = esp_codec_dev_set_in_channel_gain(record_dev_handle, mic_layout.echo_ref_mask, BOARD_ECHO_REF_GAIN_DB);
    }
    if (ret != ESP_OK) {
        (void)board_codec_dev_stop();
    }
    board_audio_unlock();
    return ret;
}

esp_err_t board_codec_volume_set(int volume, int *volume_set)
{
    (void)volume_set;
    ESP_RETURN_ON_ERROR(board_audio_output_lock(), TAG, "Lock codec volume failed");
    _vloume_intensity = volume;
    esp_err_t ret = esp_codec_dev_set_out_vol(play_dev_handle, volume);
    board_audio_output_unlock();
    ESP_RETURN_ON_ERROR(ret, TAG, "Set Codec volume failed");

    ESP_LOGI(TAG, "Setting volume: %d", volume);

    return ESP_OK;
}

int board_codec_volume_get(void)
{
    return _vloume_intensity;
}

esp_err_t board_codec_mute_set(bool enable)
{
    ESP_RETURN_ON_ERROR(board_audio_output_lock(), TAG, "Lock codec mute failed");
    esp_err_t ret = play_dev_handle ? esp_codec_dev_set_out_mute(play_dev_handle, enable) : ESP_ERR_INVALID_STATE;
    board_audio_output_unlock();
    return ret;
}

esp_err_t board_codec_dev_stop(void)
{
    ESP_RETURN_ON_ERROR(board_audio_lock(), TAG, "Lock codec stop failed");
    esp_err_t ret = ESP_OK;

    if (record_dev_handle) {
        update_codec_result(&ret, esp_codec_dev_close(record_dev_handle));
    }
    if (play_dev_handle) {
        update_codec_result(&ret, esp_codec_dev_close(play_dev_handle));
    }

    board_audio_unlock();
    return ret;
}

esp_err_t board_codec_dev_resume(void)
{
    /* Re-apply the format that was opened last. Reopening with the default
     * format made a resumed 44.1 kHz track play back at the wrong pitch. */
    return board_codec_set_fs(_out_rate, _out_bits, _out_ch);
}

esp_err_t board_codec_init(void)
{
    ESP_RETURN_ON_ERROR(board_audio_lock(), TAG, "Lock codec initialization failed");
    if (_is_audio_init) {
        board_audio_unlock();
        return ESP_OK;
    }

    esp_err_t ret = board_audio_devices_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize audio codec devices failed: %s", esp_err_to_name(ret));
        board_audio_unlock();
        return ret;
    }

    ret = board_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    _is_audio_init = true;
    board_audio_unlock();
    return ESP_OK;

cleanup:
    if (board_audio_deinit() != ESP_OK) {
        ESP_LOGE(TAG, "Clean up codec initialization failed");
    }
    board_audio_unlock();
    return ret;
}
