/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lv_adapter.h"
#include "esp_board_manager_includes.h"
#include "board_display.h"

static const char *TAG = "board_display";

#define BOARD_TOUCH_INIT_ATTEMPTS           (3)
#define BOARD_TOUCH_RESET_PULSE_MS          (10)
#define BOARD_TOUCH_RESET_SETTLE_MS         (100)
// Clockwise turn of the LVGL UI on the native 1024x600 panel; the 7B UI is upright at 180.
#define BOARD_DISPLAY_ROTATION              ESP_LV_ADAPTER_ROTATE_180

static lv_display_t *s_display = NULL;
static lv_indev_t *s_touch_indev = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static periph_ledc_handle_t *s_backlight = NULL;
static void *s_i2c_master = NULL;
static uint32_t s_backlight_duty_max = 0;
static int s_brightness = 0;

static esp_err_t board_backlight_init(void)
{
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS), TAG,
                        "Initialize lcd_brightness failed");
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, (void **)&s_backlight),
                        TAG, "Get lcd_brightness handle failed");

    dev_ledc_ctrl_config_t *dev_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, (void **)&dev_cfg),
                        TAG, "Get lcd_brightness config failed");
    periph_ledc_config_t *ledc_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_periph_get_config(dev_cfg->ledc_name, (void **)&ledc_cfg), TAG,
                        "Get LEDC config failed");

    // The LEDC peripheral applies output_invert, so a larger duty is always brighter here.
    s_backlight_duty_max = (1U << (uint32_t)ledc_cfg->duty_resolution) - 1;
    s_brightness = (int)dev_cfg->default_percent;
    return ESP_OK;
}

// Log every address that ACKs on i2c_master, to tell a silent GT911 apart from one at an unexpected address.
static void board_i2c_scan(void)
{
    if (!s_i2c_master) {
        return;
    }
    int found = 0;
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe((i2c_master_bus_handle_t)s_i2c_master, addr, 50) == ESP_OK) {
            ESP_LOGW(TAG, "I2C scan: device at 0x%02x", addr);
            found++;
        }
    }
    ESP_LOGW(TAG, "I2C scan: %d device(s) found", found);
}

/*
 * The GT911 latches its I2C address from the INT level when RESET is released. INT is not wired on
 * this board, so a reset can move it between 0x5D and 0x14: reset first, and only then let the
 * lcd_touch device probe the address.
 */
static esp_err_t board_touch_reset(void)
{
    dev_lcd_touch_config_t *touch_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, (void **)&touch_cfg), TAG,
                        "Get lcd_touch config failed");
    const gpio_num_t rst_gpio = touch_cfg->touch_config.rst_gpio_num;
    const uint32_t reset_level = touch_cfg->touch_config.levels.reset;
    ESP_RETURN_ON_FALSE(rst_gpio != GPIO_NUM_NC, ESP_ERR_INVALID_STATE, TAG, "lcd_touch has no reset GPIO");

    const gpio_config_t rst_config = {
        .pin_bit_mask = BIT64(rst_gpio),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&rst_config), TAG, "Configure touch reset failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(rst_gpio, reset_level), TAG, "Assert touch reset failed");
    vTaskDelay(pdMS_TO_TICKS(BOARD_TOUCH_RESET_PULSE_MS));
    ESP_RETURN_ON_ERROR(gpio_set_level(rst_gpio, !reset_level), TAG, "Release touch reset failed");
    vTaskDelay(pdMS_TO_TICKS(BOARD_TOUCH_RESET_SETTLE_MS));
    return ESP_OK;
}

// Overrides the weak hook of the board definition: a driver reset here, after the probe, could move
// the controller off the address that was just found.
esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *touch_dev_config,
                                    esp_lcd_touch_handle_t *ret_touch)
{
    esp_lcd_touch_config_t touch_config = *touch_dev_config;
    touch_config.rst_gpio_num = GPIO_NUM_NC;
    return esp_lcd_touch_new_i2c_gt911(io, &touch_config, ret_touch);
}

// esp_lv_adapter only turns the picture; map the native touch points into the rotated LVGL frame.
// esp_lcd_touch mirrors against the native x_max/y_max first and swaps the axes afterwards.
static void board_touch_apply_rotation(esp_lcd_touch_handle_t touch)
{
    bool swap_xy = false;
    bool mirror_x = false;
    bool mirror_y = false;
    switch (BOARD_DISPLAY_ROTATION) {
    case ESP_LV_ADAPTER_ROTATE_90:
        swap_xy = true;
        mirror_x = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_180:
        mirror_x = true;
        mirror_y = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_270:
        swap_xy = true;
        mirror_y = true;
        break;
    default:
        break;
    }
    // The GT911 frame sits half a turn from the panel frame, so its origin is the far panel corner.
    esp_lcd_touch_set_swap_xy(touch, swap_xy);
    esp_lcd_touch_set_mirror_x(touch, !mirror_x);
    esp_lcd_touch_set_mirror_y(touch, !mirror_y);
}

static lv_display_t *board_display_lvgl_init(void)
{
    dev_display_lcd_handles_t *lcd_handles = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    ESP_RETURN_ON_FALSE(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_handles) == ESP_OK
                        && lcd_handles, NULL, TAG, "Get display_lcd handle failed");
    ESP_RETURN_ON_FALSE(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_cfg) == ESP_OK
                        && lcd_cfg, NULL, TAG, "Get display_lcd config failed");
    ESP_RETURN_ON_FALSE(strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0, NULL, TAG,
                        "Unsupported LCD sub_type: %s", lcd_cfg->sub_type);

    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    // Keep LVGL on core 1 with a larger PSRAM-backed stack; high-resolution UI and
    // app switching can otherwise starve the GUI task or overflow the default stack.
    adapter_cfg.task_stack_size = 16 * 1024;
    adapter_cfg.task_priority = 10;
    adapter_cfg.task_core_id = 1;
    adapter_cfg.stack_in_psram = true;
    ESP_RETURN_ON_FALSE(esp_lv_adapter_init(&adapter_cfg) == ESP_OK, NULL, TAG, "Initialize LVGL adapter failed");

    esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                lcd_handles->panel_handle, lcd_handles->io_handle, lcd_cfg->lcd_width, lcd_cfg->lcd_height,
                BOARD_DISPLAY_ROTATION);
    // board_devices.yaml allocates three DPI frame buffers, which both the triple-buffered partial
    // mode and the adapter's rotation require.
    disp_cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Register LVGL display failed");
        goto err_adapter;
    }

    dev_lcd_touch_handles_t *touch_handles = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, (void **)&touch_handles) == ESP_OK
            && touch_handles && touch_handles->touch_handle) {
        board_touch_apply_rotation(touch_handles->touch_handle);
        const esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handles->touch_handle);
        s_touch_indev = esp_lv_adapter_register_touch(&touch_cfg);
        if (s_touch_indev) {
            s_touch = touch_handles->touch_handle;
        } else {
            ESP_LOGW(TAG, "Register LVGL touch failed, continue without touch");
        }
    }

    if (esp_lv_adapter_start() != ESP_OK) {
        ESP_LOGE(TAG, "Start LVGL adapter failed");
        goto err_disp;
    }

    s_panel = lcd_handles->panel_handle;
    ESP_LOGI(TAG, "LVGL display started: %dx%d, rotation=%d, touch=%s", (int)lv_display_get_horizontal_resolution(disp),
             (int)lv_display_get_vertical_resolution(disp), (int)BOARD_DISPLAY_ROTATION, s_touch ? "yes" : "no");
    return disp;

err_disp:
    if (s_touch_indev) {
        esp_lv_adapter_unregister_touch(s_touch_indev);
        s_touch_indev = NULL;
        s_touch = NULL;
    }
    esp_lv_adapter_unregister_display(disp);
err_adapter:
    esp_lv_adapter_deinit();
    return NULL;
}

lv_display_t *board_display_start(void)
{
    if (s_display) {
        return s_display;
    }

    ESP_RETURN_ON_FALSE(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD) == ESP_OK, NULL, TAG,
                        "Initialize display_lcd failed");

    // Keep the i2c_master reference for good so later devices do not tear the bus down.
    if (!s_i2c_master) {
        ESP_RETURN_ON_FALSE(esp_board_periph_ref_handle(ESP_BOARD_PERIPH_NAME_I2C_MASTER, &s_i2c_master) == ESP_OK,
                            NULL, TAG, "Initialize i2c_master failed");
    }

    // Give the GT911 a few chances before running without touch; every attempt starts from a reset.
    esp_err_t touch_ret = ESP_FAIL;
    for (int attempt = 1; attempt <= BOARD_TOUCH_INIT_ATTEMPTS; attempt++) {
        if (board_touch_reset() != ESP_OK) {
            ESP_LOGW(TAG, "Reset lcd_touch failed (attempt %d/%d)", attempt, BOARD_TOUCH_INIT_ATTEMPTS);
        }
        touch_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
        if (touch_ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "Initialize lcd_touch failed (attempt %d/%d)", attempt, BOARD_TOUCH_INIT_ATTEMPTS);
    }
    if (touch_ret != ESP_OK) {
        ESP_LOGW(TAG, "Initialize lcd_touch failed, continue without touch");
        board_i2c_scan();
    }
    if (board_backlight_init() != ESP_OK) {
        ESP_LOGW(TAG, "Initialize backlight failed, brightness control unavailable");
        s_backlight = NULL;
    }

    s_display = board_display_lvgl_init();
    if (!s_display) {
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS);
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
        s_backlight = NULL;
    }
    return s_display;
}

esp_lcd_panel_handle_t board_display_get_panel_handle(void)
{
    return s_panel;
}

esp_lcd_touch_handle_t board_display_get_touch_handle(void)
{
    return s_touch;
}

uint16_t board_display_get_rotation(void)
{
    return (uint16_t)BOARD_DISPLAY_ROTATION;
}

esp_err_t board_display_get_info(board_display_info_t *info)
{
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "Invalid display info pointer");

    dev_display_lcd_config_t *lcd_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_cfg), TAG,
                        "Get display_lcd config failed");
    ESP_RETURN_ON_FALSE(lcd_cfg && strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0,
                        ESP_ERR_NOT_SUPPORTED, TAG, "Only MIPI DSI panels are supported");

    uint8_t num_fbs = lcd_cfg->sub_cfg.dsi.dpi_config.num_fbs;
    if (num_fbs == 0) {
        num_fbs = 1;  // esp_lcd allocates one frame buffer when num_fbs is left at 0
    }
    *info = (board_display_info_t) {
        .width = lcd_cfg->lcd_width,
        .height = lcd_cfg->lcd_height,
        .bits_per_pixel = lcd_cfg->bits_per_pixel,
        .frame_buffer_num = num_fbs > BOARD_DISPLAY_MAX_FRAME_BUFFERS ? BOARD_DISPLAY_MAX_FRAME_BUFFERS : num_fbs,
    };
    return ESP_OK;
}

esp_err_t board_display_brightness_set(int brightness)
{
    ESP_RETURN_ON_FALSE(s_backlight, ESP_ERR_INVALID_STATE, TAG, "Backlight is not initialized");

    if (brightness > 100) {
        brightness = 100;
    } else if (brightness < 0) {
        brightness = 0;
    }

    const uint32_t duty = (uint32_t)brightness * s_backlight_duty_max / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(s_backlight->speed_mode, s_backlight->channel, duty), TAG, "Set LEDC duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(s_backlight->speed_mode, s_backlight->channel), TAG, "Update LEDC duty failed");
    s_brightness = brightness;
    return ESP_OK;
}

int board_display_brightness_get(void)
{
    return s_brightness;
}

esp_err_t board_display_backlight_on(void)
{
    return board_display_brightness_set(100);
}

esp_err_t board_display_backlight_off(void)
{
    return board_display_brightness_set(0);
}

bool board_display_lock(int32_t timeout_ms)
{
    return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
}

void board_display_unlock(void)
{
    esp_lv_adapter_unlock();
}
