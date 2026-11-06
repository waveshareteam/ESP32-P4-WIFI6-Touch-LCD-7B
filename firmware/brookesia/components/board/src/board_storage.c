/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include "esp_check.h"
#include "esp_board_manager_includes.h"
#include "board_storage.h"

static const char *TAG = "board_storage";

static bool s_spiffs_mounted = false;
static bool s_sdcard_mounted = false;

esp_err_t board_spiffs_mount(void)
{
    if (s_spiffs_mounted) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SPIFFS), TAG, "Mount SPIFFS failed");
    s_spiffs_mounted = true;
    return ESP_OK;
}

esp_err_t board_spiffs_unmount(void)
{
    if (!s_spiffs_mounted) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SPIFFS), TAG, "Unmount SPIFFS failed");
    s_spiffs_mounted = false;
    return ESP_OK;
}

esp_err_t board_sdcard_mount(void)
{
    if (s_sdcard_mounted) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD), TAG, "Mount SD card failed");
    s_sdcard_mounted = true;
    return ESP_OK;
}

esp_err_t board_sdcard_unmount(void)
{
    if (!s_sdcard_mounted) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD), TAG, "Unmount SD card failed");
    s_sdcard_mounted = false;
    return ESP_OK;
}
