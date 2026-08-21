/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief USB extended-screen panel glue
 *
 * This file offers API for basic LCD control.
 * It is useful for users who want to use the LCD without the default Graphical Library LVGL.
 *
 * LVGL integration is owned by this example's application layer; this public
 * header intentionally exposes only the low-level panel and touch handles.
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frame-buffer configuration for the USB extended-screen panel
 *
 */
typedef struct {
    int dpi_fb_buf_num;
} usb_extend_display_config_t;

/**
 * @brief LCD transmit done callback function type
 *
 */
typedef bool (*usb_extend_display_on_trans_done_cb_t)(esp_lcd_panel_handle_t handle);
typedef bool (*usb_extend_display_on_vsync_cb_t)(esp_lcd_panel_handle_t handle);

typedef struct {
    usb_extend_display_on_trans_done_cb_t on_trans_done_cb;
    usb_extend_display_on_vsync_cb_t on_vsync_cb;
} usb_extend_display_callback_t;

/**
 * @brief Create new display panel
 *
 * For maximum flexibility, this function performs only reset and initialization of the display.
 * You must turn on the display explicitly by calling esp_lcd_panel_disp_on_off().
 * The display's backlight is not turned on either. You can use bsp_display_backlight_on/off(),
 * bsp_display_brightness_set() (on supported boards) or implement your own backlight control.
 *
 * If you want to free resources allocated by this function, you can use esp_lcd API, ie.:
 *
 * \code{.c}
 * esp_lcd_panel_del(panel);
 * esp_lcd_panel_io_del(io);
 * spi_bus_free(spi_num_from_configuration);
 * \endcode
 *
 * @param[in]  config    display configuration. Set to NULL if not needed.
 * @param[out] ret_panel esp_lcd panel handle. Set to NULL if not needed.
 * @param[out] ret_io    esp_lcd IO handle. Set to NULL if not needed.
 * @return
 *      - ESP_OK         On success
 *      - Else           esp_lcd failure
 */
esp_err_t usb_extend_display_new(const usb_extend_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io);

/**
 * @brief Register a callback function which will be called when finish refreshing
 *
 * @param[in] callback The function to be registered. It should return true if need to yield, otherwise return false
 *
 * @return
 *      - ESP_OK               Succsee
 *      - ESP_ERR_INVALID_ARG  Callback function should be placed in IRAM, use `IRAM_ATTR` to define it
 */
esp_err_t usb_extend_display_register_callback(usb_extend_display_callback_t *callback);

esp_err_t usb_extend_touch_new(esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
