# ESP-Brookesia Phone

[简体中文](README_ZH.md)

This example runs the ESP-Brookesia phone-style UI on the
ESP32-P4-WIFI6-Touch-LCD-7B board. It uses the LVGL 9 Brookesia core and keeps
one small Squareline example app in the launcher.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for power, flashing, and serial monitor.

## Build and Flash

```bash
cd examples/esp-idf/11_esp_brookesia_phone
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the board's serial port.

## Board Adaptation

- Uses the Registry-managed `waveshare/esp32_p4_wifi6_touch_lcd_7b` BSP at `3.0.0`.
- Uses the BSP display configuration and lock APIs with the 7B panel rotation and touch orientation.
- Uses the local `components/brookesia_core` component from the LVGL 9 Brookesia integration.
- Starts the phone system from its app registry and installs the single
  `brookesia_app_squareline_demo` example app.
- Uses the 1024 x 600 stylesheet.

The old camera, audio-player, video-player, hosted Wi-Fi, and media-resource
apps are intentionally not part of this example. Hardware-specific features
remain available in their dedicated examples and firmware projects.

## CI Compatibility

The example is designed for the ESP-IDF 5.5+ BSP contract. The Brookesia core
pins LVGL `9.5.0` and keeps the core and app source in the repository so the
example does not depend on the legacy ESP-Brookesia LVGL 8 package.

GitHub Actions verifies compilation only; display and touch runtime behavior
still requires the physical 7B board.

Generated files such as `sdkconfig`, `build/`, `managed_components/`, and
`dependencies.lock` are ignored and should not be committed.
