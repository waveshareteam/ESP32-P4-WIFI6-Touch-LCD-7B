# ESP-Brookesia Phone

[中文](README_CN.md)

This example runs an ESP-Brookesia phone-style UI on
ESP32-P4-WIFI6-Touch-LCD-7B. It demonstrates display, touch, audio, camera,
hosted Wi-Fi, SPIFFS media assets, and optional SD-card video playback on the
7-inch 1024 x 600 board.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for power, flashing, and serial monitor.
- Optional SD card for MJPEG video playback.

## Build and Flash

```bash
cd examples/ESP-IDF/11_esp_brookesia_phone
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Board Adaptation

- Uses `waveshare/esp32_p4_wifi6_touch_lcd_7b`.
- Uses the 1024 x 600 Brookesia stylesheet.
- Uses `BSP_LCD_H_RES` and `BSP_LCD_V_RES` for display and camera sizing.
- The About screen identifies the board as `ESP32-P4-WIFI6-Touch-LCD-7B`.

## Optional Video Playback

To enable the Video Player app, insert an SD card and enable the SD-card option
in menuconfig:

```bash
idf.py menuconfig
```

Then select `Example Configuration` > `Enable SD Card`.

Video files should be MJPEG. A typical conversion command is:

```bash
ffmpeg -i input.mp4 -vcodec mjpeg -q:v 2 -vf "scale=1024:600" -acodec copy output.mjpeg
```

## Media Assets

The `spiffs/` directory contains sample audio assets used by the demo apps.
Replace these assets with properly licensed customer content before publishing a
derived product firmware.

Generated files such as `sdkconfig`, `build/`, `managed_components/`, and
`dependencies.lock` are ignored and should not be committed.
