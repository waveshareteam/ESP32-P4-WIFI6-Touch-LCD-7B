/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "audio_player.h"
#include "esp_err.h"

esp_err_t board_audio_player_mute(AUDIO_PLAYER_MUTE_SETTING setting);
