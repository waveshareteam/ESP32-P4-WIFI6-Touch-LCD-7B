# LVGL Demo v9

[简体中文](README_ZH.md)

This example runs the LVGL v9 widgets demo on ESP32-P4-WIFI6-Touch-LCD-7B. It
uses the board BSP for the 1024 x 600 EK79007 MIPI-DSI panel and GT911 touch
controller.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for power, flashing, and serial monitor.

## Build and Flash

```bash
cd examples/esp-idf/09_lvgl_demo_v9
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Notes

- The example depends on `lvgl/lvgl` v9.2.x.
- `main/main.c` enables `lv_demo_widgets()`. You can switch to other LVGL demos
  by changing that call.
- Use this as the preferred starting point for new LVGL applications.

Generated files such as `sdkconfig`, `build/`, `managed_components/`, and
`dependencies.lock` are ignored and should not be committed.
