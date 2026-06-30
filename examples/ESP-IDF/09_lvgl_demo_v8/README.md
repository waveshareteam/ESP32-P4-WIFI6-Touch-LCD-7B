# LVGL Demo v8

[中文版本](./README_CN.md)

This example runs the LVGL v8 demo on ESP32-P4-WIFI6-Touch-LCD-7B. It uses the
board BSP for the 1024 x 600 EK79007 MIPI-DSI panel and GT911 touch controller.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for power, flashing, and serial monitor.

## Build and Flash

```bash
cd examples/ESP-IDF/09_lvgl_demo_v8
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Notes

- The example depends on `lvgl/lvgl` v8.4.x.
- The display is rotated through the BSP display API in `main/main.c`.
- Use this example when maintaining an LVGL v8 application.
- For new UI work, also evaluate [10_lvgl_demo_v9](../10_lvgl_demo_v9/).

Generated files such as `sdkconfig`, `build/`, `managed_components/`, and
`dependencies.lock` are ignored and should not be committed.
