/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * Board display interface
 * LCD, touch and backlight from esp_board_manager, driven by LVGL through esp_lv_adapter.
 **************************************************************************************************/
/**
 * @brief Lock LVGL through esp_lv_adapter.
 *
 *
 * @param timeout_ms Maximum wait in milliseconds; negative waits forever.
 * @return true when the lock was acquired.
 */
bool board_display_lock(int32_t timeout_ms);

/** @brief Release the LVGL lock acquired by board_display_lock(). */
void board_display_unlock(void);

/**
 * @brief Initialize the LCD, touch and backlight through esp_board_manager and start LVGL.
 *
 * Uses the display_lcd, lcd_touch and lcd_brightness devices of the board definition. Touch and
 * backlight are optional; the display is still started when they fail. Repeated calls return the
 * already started display.
 *
 * @return LVGL display handle, or NULL on failure
 */
lv_display_t *board_display_start(void);

/** @brief Get the LCD panel handle; NULL before board_display_start(). */
esp_lcd_panel_handle_t board_display_get_panel_handle(void);

/**
 * @brief Get the touch driver handle that LVGL polls; NULL when touch is unavailable.
 *
 * Read it directly only while LVGL is paused, so the two readers never race on the controller.
 */
esp_lcd_touch_handle_t board_display_get_touch_handle(void);

/**
 * @brief Get the clockwise rotation, in degrees (0, 90, 180 or 270), LVGL applies to the panel.
 *
 * board_display_get_info() reports the native panel geometry. Code that writes the DPI frame
 * buffers directly must turn its picture by this angle to match the LVGL orientation.
 */
uint16_t board_display_get_rotation(void);

/** Upper bound of DPI frame buffers a MIPI DSI panel can own. */
#define BOARD_DISPLAY_MAX_FRAME_BUFFERS    (3)

/** @brief LCD geometry and pixel format from the display_lcd device in board_devices.yaml. */
typedef struct {
    uint32_t width;             /*!< Horizontal resolution in pixels */
    uint32_t height;            /*!< Vertical resolution in pixels */
    uint8_t  bits_per_pixel;    /*!< 16 for RGB565, 24 for RGB888 */
    uint8_t  frame_buffer_num;  /*!< DPI frame buffers, at most BOARD_DISPLAY_MAX_FRAME_BUFFERS */
} board_display_info_t;

/**
 * @brief Get the LCD geometry and pixel format.
 *
 * @param[out] info Display information
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: info is NULL
 *    - Others: The display_lcd configuration is unavailable
 */
esp_err_t board_display_get_info(board_display_info_t *info);

/**
 * @brief Set display brightness.
 *
 * @param brightness Brightness in percent, clamped to 0-100
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_STATE: Backlight is not initialized
 *    - Others: LEDC error
 */
esp_err_t board_display_brightness_set(int brightness);

/**
 * @brief Get display brightness.
 *
 * @return
 *   - brightness in percent
 */
int board_display_brightness_get(void);

/** @brief Set backlight to 100%. */
esp_err_t board_display_backlight_on(void);

/** @brief Set backlight to 0%. */
esp_err_t board_display_backlight_off(void);

#ifdef __cplusplus
}
#endif
