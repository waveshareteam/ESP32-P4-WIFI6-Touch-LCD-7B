# How To Create Project

Minimal ESP-IDF project template for ESP32-P4-WIFI6-Touch-LCD-7B.

This example contains only the project boilerplate and an empty `app_main()`.
Use it as a clean starting point when creating a new board-specific ESP-IDF
application.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.

## Build

```bash
cd examples/esp-idf/01_how_to_create_project
idf.py set-target esp32p4
idf.py build
```

## Reuse Pattern

- Keep the root `CMakeLists.txt`.
- Put application source under `main/`.
- Add board dependencies in `main/idf_component.yml` when the app needs the BSP.
- Keep generated files such as `sdkconfig`, `build/`, and `managed_components/`
  out of git.
