/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mount point of the "storage" SPIFFS partition, as set by the fs_spiffs device in board_devices.yaml. */
#define BOARD_SPIFFS_MOUNT_POINT    "/spiffs"

/**
 * @brief Mount the SPIFFS "storage" partition through the esp_board_manager fs_spiffs device.
 *
 * Repeated calls return ESP_OK while the partition is mounted.
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Mount failed
 */
esp_err_t board_spiffs_mount(void);

/** @brief Unmount the SPIFFS partition mounted by board_spiffs_mount(). */
esp_err_t board_spiffs_unmount(void);

/* Mount point of the SD card, as set by the fs_sdcard device in board_devices.yaml. */
#define BOARD_SD_MOUNT_POINT        "/sdcard"

/**
 * @brief Mount the SD card through the esp_board_manager fs_sdcard device.
 *
 * Repeated calls return ESP_OK while the card is mounted.
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Mount failed
 */
esp_err_t board_sdcard_mount(void);

/** @brief Unmount the SD card mounted by board_sdcard_mount(). */
esp_err_t board_sdcard_unmount(void);

#ifdef __cplusplus
}
#endif
