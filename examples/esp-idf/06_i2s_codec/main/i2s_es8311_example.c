/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "example_config.h"

static const char *TAG = "i2s_codec";
static esp_codec_dev_handle_t speaker_codec;
#if CONFIG_EXAMPLE_MODE_ECHO
static esp_codec_dev_handle_t microphone_codec;
#endif

#if CONFIG_EXAMPLE_MODE_MUSIC
extern const uint8_t music_pcm_start[] asm("_binary_canon_pcm_start");
extern const uint8_t music_pcm_end[] asm("_binary_canon_pcm_end");
#endif

static esp_err_t codec_init(void)
{
    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = EXAMPLE_SAMPLE_RATE,
        .channel = EXAMPLE_CHANNEL_COUNT,
        .bits_per_sample = EXAMPLE_BITS_PER_SAMPLE,
    };

    speaker_codec = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(speaker_codec != NULL, ESP_FAIL, TAG, "speaker codec init failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(speaker_codec, &sample_info),
                        TAG, "speaker codec open failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(speaker_codec, CONFIG_EXAMPLE_VOICE_VOLUME),
                        TAG, "speaker volume setup failed");

#if CONFIG_EXAMPLE_MODE_ECHO
    microphone_codec = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(microphone_codec != NULL, ESP_FAIL, TAG, "microphone codec init failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(microphone_codec, &sample_info),
                        TAG, "microphone codec open failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_in_gain(microphone_codec, CONFIG_EXAMPLE_MIC_GAIN_DB),
                        TAG, "microphone gain setup failed");
#endif

    return ESP_OK;
}

#if CONFIG_EXAMPLE_MODE_MUSIC
static void codec_music(void *args)
{
    (void)args;
    const size_t music_size = music_pcm_end - music_pcm_start;

    while (true) {
        esp_err_t ret = esp_codec_dev_write(speaker_codec, (void *)music_pcm_start, music_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "PCM playback failed: %s", esp_err_to_name(ret));
            abort();
        }
        ESP_LOGI(TAG, "played %u PCM bytes", (unsigned int)music_size);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#else
static void codec_echo(void *args)
{
    (void)args;
    uint8_t *audio_data = malloc(EXAMPLE_RECV_BUF_SIZE);
    if (audio_data == NULL) {
        ESP_LOGE(TAG, "unable to allocate the echo buffer");
        abort();
    }

    ESP_LOGI(TAG, "microphone-to-speaker echo started");
    while (true) {
        esp_err_t ret = esp_codec_dev_read(microphone_codec, audio_data, EXAMPLE_RECV_BUF_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "microphone read failed: %s", esp_err_to_name(ret));
            abort();
        }
        ret = esp_codec_dev_write(speaker_codec, audio_data, EXAMPLE_RECV_BUF_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "speaker write failed: %s", esp_err_to_name(ret));
            abort();
        }
    }
}
#endif

void app_main(void)
{
    ESP_ERROR_CHECK(codec_init());

#if CONFIG_EXAMPLE_MODE_MUSIC
    BaseType_t created = xTaskCreate(codec_music, "codec_music", 4096, NULL, 5, NULL);
#else
    BaseType_t created = xTaskCreate(codec_echo, "codec_echo", 8192, NULL, 5, NULL);
#endif
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "audio codec initialization succeeded");
}
