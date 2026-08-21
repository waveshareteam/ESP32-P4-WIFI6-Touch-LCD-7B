/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_touch_ft5x06.h"

#include "sdkconfig.h"
#include "usb_extend_err_check.h"
#include "bsp/esp-bsp.h"
#include "usb_extend_support/panel.h"

#include "driver/i2c_master.h"

static const char *TAG = "usb_extend_panel";
static usb_extend_display_on_trans_done_cb_t trans_done_cb;
static usb_extend_display_on_vsync_cb_t vsync_cb;
static esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
static esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;
static esp_lcd_panel_handle_t mipi_dpi_panel = NULL;

static i2c_master_bus_handle_t i2c_bus_handle;

/**************************************************************************************************
 *
 * Display Panel Function
 *
 **************************************************************************************************/
IRAM_ATTR static bool on_color_trans_done(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
    if (trans_done_cb) {
        if (trans_done_cb(panel)) {
            need_yield = pdTRUE;
        }
    }

    return (need_yield == pdTRUE);
}

IRAM_ATTR static bool on_vsync(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
    if (vsync_cb) {
        if (vsync_cb(panel)) {
            need_yield = pdTRUE;
        }
    }

    return (need_yield == pdTRUE);
}

esp_err_t usb_extend_display_new(const usb_extend_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    ESP_RETURN_ON_FALSE(config == NULL || config->dpi_fb_buf_num == CONFIG_BSP_LCD_DPI_BUFFER_NUMS,
                        ESP_ERR_INVALID_ARG, TAG, "dpi frame-buffer count must match BSP configuration");

    bsp_lcd_handles_t lcd_handles = {0};
    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_new_with_handles(NULL, &lcd_handles));
    mipi_dsi_bus = lcd_handles.mipi_dsi_bus;
    mipi_dbi_io = lcd_handles.io;
    mipi_dpi_panel = lcd_handles.panel;

    // register event callbacks
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = on_color_trans_done,
        .on_refresh_done = on_vsync,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(mipi_dpi_panel, &cbs, NULL));

    if (ret_io) {
        *ret_io = mipi_dbi_io;
    }
    if (ret_panel) {
        *ret_panel = mipi_dpi_panel;
    }

    return ESP_OK;
}

esp_err_t usb_extend_display_register_callback(usb_extend_display_callback_t *callback)
{
#if CONFIG_LCD_RGB_ISR_IRAM_SAFE
    if (callback) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(callback), ESP_ERR_INVALID_ARG, TAG, "Callback not in IRAM");
    }
#endif
    trans_done_cb = callback->on_trans_done_cb;
    vsync_cb = callback->on_vsync_cb;

    return ESP_OK;
}

/**************************************************************************************************
 *
 * Touch Panel Function
 *
 **************************************************************************************************/

esp_err_t usb_extend_touch_new(esp_lcd_touch_handle_t *ret_touch)
{
    /* Initialize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());
    i2c_bus_handle = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(i2c_bus_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "BSP I2C bus is unavailable");

    /* Initialize touch */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            // Match the managed Waveshare BSP touch-orientation defaults.
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    bool is_gt911 = false;

    if (ESP_OK == i2c_master_probe(i2c_bus_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 100)) {
        ESP_LOGI(TAG, "Found touch GT911");
        is_gt911 = true;
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        memcpy(&tp_io_config, &config, sizeof(config));
        tp_io_config.scl_speed_hz = 400000;
    } else if (ESP_OK == i2c_master_probe(i2c_bus_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 100)) {
        ESP_LOGI(TAG, "Found touch GT911");
        is_gt911 = true;
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
        memcpy(&tp_io_config, &config, sizeof(config));
        tp_io_config.scl_speed_hz = 400000;
    } else if (ESP_OK == i2c_master_probe(i2c_bus_handle, ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS, 100)) {
        ESP_LOGI(TAG, "Found touch FT5x06");
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        memcpy(&tp_io_config, &config, sizeof(config));
    } else {
        ESP_LOGE(TAG, "Touch not found");
        return ESP_FAIL;
    }

    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle));

    if (is_gt911) {
        esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
            .dev_addr = tp_io_config.dev_addr,
        };
        tp_cfg.driver_data = &tp_gt911_config;
        return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
    }
    return esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch);
}
