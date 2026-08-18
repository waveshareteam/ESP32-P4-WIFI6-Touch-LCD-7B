/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "usb_extend_support/panel.h"
#include "esp_lcd_touch.h"
#include "app_usb.h"
#include "usb_descriptors.h"

static const char *TAG = "app_touch";

static esp_lcd_touch_handle_t tp = NULL;

static void app_touch_task(void *arg)
{
    esp_lcd_touch_point_data_t touch_data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];
    uint8_t touchpad_cnt = 0;
    bool send_press = false;
    while (1) {
        esp_lcd_touch_read_data(tp);
        bool touchpad_pressed = esp_lcd_touch_get_data(tp, touch_data, &touchpad_cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS) == ESP_OK;
        hid_report_t report = {0};
        if (touchpad_pressed && touchpad_cnt > 0) {
            report.report_id = REPORT_ID_TOUCH;
            int i = 0;
            for (i = 0; i < touchpad_cnt; i++) {
                report.touch_report.data[i].index = touch_data[i].track_id;
                report.touch_report.data[i].press_down = 1;
                report.touch_report.data[i].x = touch_data[i].x;
                report.touch_report.data[i].y = touch_data[i].y;
                report.touch_report.data[i].width = touch_data[i].strength;
                report.touch_report.data[i].height = touch_data[i].strength;
                /*!< >= LOG_LEVEL_DEBUG */
#if CONFIG_LOG_DEFAULT_LEVEL >= 4
                /*!< For debug */
                printf("(%d: %d, %d. %d) ", touch_data[i].track_id, touch_data[i].x, touch_data[i].y, touch_data[i].strength);
#endif
            }
#if CONFIG_LOG_DEFAULT_LEVEL >= 4
            /*!< For debug */
            printf("\n");
#endif
            ESP_LOGD(TAG, "touchpad cnt: %d\n", touchpad_cnt);
            report.touch_report.cnt = touchpad_cnt;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            send_press = true;
        } else if (send_press) {
            send_press = false;
            report.report_id = REPORT_ID_TOUCH;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            ESP_LOGD(TAG, "send release %d", touchpad_cnt);
        }
        // Reading from the GT911 at a time shorter than this may result in false reports.
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

esp_err_t app_touch_init(void)
{
    usb_extend_touch_new(&tp);
    xTaskCreate(app_touch_task, "app_touch_task", 4096, NULL, CONFIG_TOUCH_TASK_PRIORITY, NULL);
    return ESP_OK;
}
