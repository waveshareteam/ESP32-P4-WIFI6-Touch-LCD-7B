/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_touch_ft5x06.h"

#include "sdkconfig.h"
#include "bsp_err_check.h"
#include "bsp/display.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_7b_usb_bsp.h"
#include "bsp/touch.h"

#include "driver/i2c_master.h"

static const char *TAG = "bsp_sub_board";
static bsp_display_on_trans_done_cb_t trans_done_cb;
static bsp_display_on_vsync_cb_t vsync_cb;
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

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    bsp_ldo_power_on();

    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    esp_lcd_dbi_io_config_t dbi_config = EK79007_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    #if ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(6, 0, 0)
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(MIPI_DPI_PX_FORMAT);
    #else
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(MIPI_DPI_PX_FORMAT);
    #endif
    if (config != NULL) {
        dpi_config.num_fbs = config->dpi_fb_buf_num;
    }
    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = 33,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_COLOR_DEPTH,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(mipi_dbi_io, &panel_config, &mipi_dpi_panel));

    gpio_set_level(33, 1);
    // register event callbacks
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = on_color_trans_done,
        .on_refresh_done = on_vsync,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(mipi_dpi_panel, &cbs, NULL));
    ESP_ERROR_CHECK(esp_lcd_panel_init(mipi_dpi_panel));

    if (ret_io) {
        *ret_io = mipi_dbi_io;
    }
    if (ret_panel) {
        *ret_panel = mipi_dpi_panel;
    }

    return ESP_OK;
}

esp_err_t bsp_display_register_callback(bsp_display_callback_t *callback)
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

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    /* Initialize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());
    BSP_ERROR_CHECK_RETURN_ERR(bsp_get_i2c_bus_handle(&i2c_bus_handle));

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
            .mirror_x = BSP_TOUCH_MIRROR_X,
            .mirror_y = BSP_TOUCH_MIRROR_Y,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

    if (ESP_OK == i2c_master_probe(i2c_bus_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 100)) {
        ESP_LOGI(TAG, "Found touch GT911");
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        memcpy(&tp_io_config, &config, sizeof(config));
        tp_io_config.scl_speed_hz = 400000;
    } else if (ESP_OK == i2c_master_probe(i2c_bus_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 100)) {
        ESP_LOGI(TAG, "Found touch GT911");
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

    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
}
