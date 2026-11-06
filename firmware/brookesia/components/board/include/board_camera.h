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

/**
 * @brief Start the MIPI CSI camera driver through the esp_board_manager camera device.
 *
 * The device shares the i2c_master bus for SCCB and the ldo_mipi supply with the display.
 * Repeated calls return the same video node while the driver stays initialized.
 *
 * @param[out] dev_path Video capture node to open(), for example "/dev/video0"; may be NULL
 * @return
 *    - ESP_OK: Success
 *    - Others: Camera driver initialization failed
 */
esp_err_t board_camera_init(const char **dev_path);

/** @brief Stop the camera driver started by board_camera_init(). */
esp_err_t board_camera_deinit(void);

#ifdef __cplusplus
}
#endif
