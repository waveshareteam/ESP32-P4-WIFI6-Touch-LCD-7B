# LVGL Display Panel

[中文](README_CN.md)

This example validates the 1024 x 600 display and GT911 touch controller with a
small LVGL app. It shows red, green, and blue screens, then switches to a white
touch canvas where touch points are drawn as black squares.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for power, flashing, and serial monitor.

## Build and Flash

```bash
cd examples/ESP-IDF/08_lvgl_display_panel
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Expected Behavior

The LCD cycles through red, green, and blue, then displays a white screen.
Touching the panel should draw black squares at the touch coordinates.

Use this example when checking display color output, touch coordinate mapping,
and LVGL input handling before moving to the larger LVGL demos.
