# Getting Started

This board uses the ESP-IDF target `esp32p4` and the
`waveshare/esp32_p4_wifi6_touch_lcd_7b` BSP.

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
cd examples/esp-idf/10_lvgl_demo_v9
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
