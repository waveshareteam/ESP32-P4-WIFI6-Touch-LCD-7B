/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/i2s_std.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_DEFAULT_SAMPLE_RATE 16000
#define CODEC_DEFAULT_BIT_WIDTH   16
#define CODEC_DEFAULT_CHANNEL     I2S_SLOT_MODE_STEREO
#define CODEC_DEFAULT_VOLUME      60

esp_err_t bsp_extra_codec_init(void);
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);
esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms);
esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
