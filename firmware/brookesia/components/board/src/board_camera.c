/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include "esp_check.h"
#include "esp_board_manager_includes.h"
#include "board_camera.h"

static const char *TAG = "board_camera";

static dev_camera_handle_t *s_camera = NULL;

esp_err_t board_camera_init(const char **dev_path)
{
    if (!s_camera) {
        ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA), TAG, "Initialize camera failed");
        dev_camera_handle_t *camera = NULL;
        esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_CAMERA, (void **)&camera);
        if (ret != ESP_OK || !camera || !camera->dev_path) {
            ESP_LOGE(TAG, "Get camera handle failed");
            esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
            return ret != ESP_OK ? ret : ESP_ERR_INVALID_STATE;
        }
        s_camera = camera;
    }
    if (dev_path) {
        *dev_path = s_camera->dev_path;
    }
    return ESP_OK;
}

esp_err_t board_camera_deinit(void)
{
    if (!s_camera) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA), TAG, "Deinitialize camera failed");
    s_camera = NULL;
    return ESP_OK;
}
