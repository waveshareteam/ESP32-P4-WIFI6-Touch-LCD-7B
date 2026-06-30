# Color Panel

Low-level EK79007 MIPI-DSI color-panel test for the 1024 x 600 LCD.

Use this example when validating the LCD panel driver before moving to LVGL.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.

## Build and Flash

```bash
cd examples/esp-idf/07_color_panel
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## Notes

- The panel resolution is configured as 1024 x 600.
- The example uses the EK79007 MIPI-DSI panel driver directly.
- Use [08_lvgl_display_panel](../08_lvgl_display_panel/) to validate touch with
  LVGL after this low-level display check passes.
