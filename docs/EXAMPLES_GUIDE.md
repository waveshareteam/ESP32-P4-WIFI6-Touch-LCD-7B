# Example Guide

Use this guide to choose examples by customer task.

## First Power-On

Start with `examples/ESP-IDF/00_board_check`. It requires only USB and confirms
that the ESP-IDF target, flash, PSRAM, chip revision, flashing flow, and serial
monitor are working.

Then run:

- `examples/ESP-IDF/02_HelloWorld` for a minimal ESP-IDF application.
- `examples/ESP-IDF/15_nvs_counter` for persistent settings.
- `examples/ESP-IDF/16_freertos_tasks` for basic task and queue patterns.

## Display and Touch

Use these examples when validating the 7-inch 1024 x 600 panel and GT911 touch:

- `examples/ESP-IDF/07_color_panel`
- `examples/ESP-IDF/08_lvgl_display_panel`
- `examples/ESP-IDF/09_lvgl_demo_v8`
- `examples/ESP-IDF/10_lvgl_demo_v9`

New display code should use `BSP_LCD_H_RES` and `BSP_LCD_V_RES` from
`waveshare/esp32_p4_wifi6_touch_lcd_7b`.

## UI and Firmware

Use `examples/ESP-IDF/11_esp_brookesia_phone` for a full ESP-Brookesia UI
example. Use `Firmware/brookesia` when you want the factory-style firmware
source project.

The Brookesia projects are adapted for the 1024 x 600 7B panel. Replace sample
media and customer-facing strings before shipping a customized product firmware.

## Peripherals

| Task | Example |
| --- | --- |
| I2C scan and bring-up | `examples/ESP-IDF/03_i2c_tools` |
| SD card | `examples/ESP-IDF/04_sdmmc` |
| Hosted Wi-Fi | `examples/ESP-IDF/05_wifistation` |
| Audio codec | `examples/ESP-IDF/06_I2SCodec` |
| USB extended display | `examples/ESP-IDF/12_usb_extend_screen` |
| RS485 | `examples/ESP-IDF/13_RS485_Test` |
| CAN/TWAI | `examples/ESP-IDF/14_TWAItransmit` |

Peripheral examples require the matching hardware path to be connected.
