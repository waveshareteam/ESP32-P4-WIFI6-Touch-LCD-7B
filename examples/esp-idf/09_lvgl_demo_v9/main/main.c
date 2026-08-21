/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_err.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lv_demos.h"

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_180,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    lv_display_t *display = bsp_display_start_with_config(&cfg);
    ESP_ERROR_CHECK(display != NULL ? ESP_OK : ESP_FAIL);

    bsp_display_backlight_on();

    ESP_ERROR_CHECK(bsp_display_lock(-1) ? ESP_OK : ESP_ERR_TIMEOUT);

    // lv_demo_music();
    // lv_demo_benchmark();
    lv_demo_widgets();

    bsp_display_unlock();
}
