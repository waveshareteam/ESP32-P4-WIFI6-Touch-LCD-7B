# Getting Started

[简体中文](GETTING_STARTED_ZH.md)

This board uses the ESP-IDF target `esp32p4` and the
[`waveshare/esp32_p4_wifi6_touch_lcd_7b` BSP](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_7b/versions/3.0.1)
version 3.0.1 from the ESP Component Registry. It supplies
`esp_lvgl_adapter` 0.6.x for LVGL integration; do not add the legacy
`esp_lvgl_port` beside it.

## Recommended Checks

1. Build the board check example.
2. Build one display example.
3. Build the Brookesia firmware source.

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
```

```bash
cd examples/esp-idf/09_lvgl_demo_v9
idf.py set-target esp32p4
idf.py build
```

```bash
cd firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

Flash with:

```bash
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port for your board.

## Display Notes

The 7B hardware is 1024 x 600. Do not reuse ESP32-P4-platform's 800 x 1280 or
10.1-inch BSP defaults for this board. Use `BSP_LCD_H_RES` and
`BSP_LCD_V_RES` in new display, touch, camera, and video code.
