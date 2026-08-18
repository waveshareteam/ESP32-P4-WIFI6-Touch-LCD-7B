# ESP-Brookesia Phone

[简体中文](README_ZH.md)

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
cd examples/esp-idf/11_esp_brookesia_phone
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Board Adaptation

- Pins the Registry-managed `waveshare/esp32_p4_wifi6_touch_lcd_7b` BSP at `3.0.0`; no local BSP copy or committed dependency override is used.
- Uses the BSP's public high-level display configuration and locking APIs, backed by its managed `espressif/esp_lvgl_adapter` `~0.6` dependency line.
- ESP-Brookesia `0.4.2` keeps its LVGL 8 contract (`>=8.3,<9`); BSP releases used by this project must preserve both the LVGL 8 and LVGL 9 public API variants.
- Uses the 1024 x 600 Brookesia stylesheet.
- Uses `BSP_LCD_H_RES` and `BSP_LCD_V_RES` for display and camera sizing.
- The About screen identifies the board as `ESP32-P4-WIFI6-Touch-LCD-7B`.

## CI Compatibility Boundary

This example is covered by the ESP-IDF v5.5.5 and v6.0.2 compilation matrix.
The upstream `esp-audio-player` 1.1.0 pin and explicit split-driver dependencies
keep this project's sources buildable. Separately, the `Product firmware`
workflow builds [`firmware/brookesia`](../../../firmware/brookesia/) on ESP-IDF
v5.5.5 exactly twice, once for `rev1_3` and once for `rev3_x`.
Legacy `esp_video`/`esp_ipa` compile shims preserve source compatibility only;
they do not verify video, ISP, or other hardware runtime behavior.
The official managed `espressif/esp_lcd_ek79007` `2.0.2~1` dependency provides
the version-gated ESP-IDF 5/6 API required by the managed Waveshare BSP 3.0.0
display-driver contract.
The official managed `espressif/human_face_detect` `0.5.0`,
`espressif/pedestrian_detect` `0.3.2`, and `espressif/esp-dl` `3.3.9`
dependencies provide ESP-IDF 5/6-compatible detector crypto/API support.
GitHub Actions verifies compilation only; LCD, face, pedestrian, and other
hardware runtime behavior remains unverified.

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

> [!NOTE]
> This example is temporarily outside the default CI matrix:
> esp-brookesia `0.4.x` requires LVGL 8 while the 7B BSP moved to the LVGL 9.5
> line. CI coverage returns once esp-brookesia publishes an LVGL 9 release.
