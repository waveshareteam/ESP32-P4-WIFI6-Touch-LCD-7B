# Example Guide

Use this guide to choose examples by customer task.

## First Power-On

Start with `examples/esp-idf/00_board_check`. It requires only USB and confirms
that the ESP-IDF target, flash, PSRAM, chip revision, flashing flow, and serial
monitor are working.

Then run:

- `examples/esp-idf/02_hello_world` for a minimal ESP-IDF application.
- `examples/esp-idf/15_nvs_counter` for persistent settings.
- `examples/esp-idf/16_freertos_tasks` for basic task and queue patterns.

## Display and Touch

Use these examples when validating the 7-inch 1024 x 600 panel and GT911 touch:

- `examples/esp-idf/07_color_panel`
- `examples/esp-idf/08_lvgl_display_panel`
- `examples/esp-idf/09_lvgl_demo_v8`
- `examples/esp-idf/10_lvgl_demo_v9`

New display code should use `BSP_LCD_H_RES` and `BSP_LCD_V_RES` from
`waveshare/esp32_p4_wifi6_touch_lcd_7b`.

## UI and Firmware

Use `examples/esp-idf/11_esp_brookesia_phone` for a full ESP-Brookesia UI
example. Use `firmware/brookesia` when you want the factory-style firmware
source project.

The Brookesia projects are adapted for the 1024 x 600 7B panel. Replace sample
media and customer-facing strings before shipping a customized product firmware.

## Peripherals

| Task | Example |
| --- | --- |
| I2C scan and bring-up | `examples/esp-idf/03_i2c_tools` |
| SD card | `examples/esp-idf/04_sdmmc` |
| Hosted Wi-Fi | `examples/esp-idf/05_wifistation` |
| Audio codec | `examples/esp-idf/06_i2s_codec` |
| MP4 or AVI playback | `examples/esp-idf/18_mp4_player` |
| USB extended display | `examples/esp-idf/12_usb_extend_screen` |
| RS485 | `examples/esp-idf/13_rs485_test` |
| CAN/TWAI | `examples/esp-idf/14_twai_transmit` |

Peripheral examples require the matching hardware path to be connected.
