# Firmware

This directory contains factory firmware artifacts and source for
ESP32-P4-WIFI6-Touch-LCD-7B.

## Layout

| Path | Purpose |
| --- | --- |
| `ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin` | Existing prebuilt factory-only binary |
| `brookesia/` | Buildable ESP-Brookesia firmware source |

## Brookesia Source

`brookesia/` is based on the local `11_esp_brookesia_phone` example and follows
the ESP32-P4-platform firmware-source layout. It is adapted for:

- `waveshare/esp32_p4_wifi6_touch_lcd_7b`
- 1024 x 600 display resolution
- GT911 touch
- ESP32-C6 hosted Wi-Fi
- Board audio, SD card, camera, and LVGL/Brookesia app flow

Build:

```bash
cd Firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

The project is included in CI together with the ESP-IDF examples. It currently
builds in CI with ESP-IDF `v5.5.4`; the hosted Wi-Fi dependency stack needs a
v6-compatible update before enabling the `v6.0.1` matrix entry.

## Media Assets

`brookesia/spiffs/` contains sample audio assets used by the music and game
demo apps. Replace those files with properly licensed customer content before
publishing a derived product firmware.
