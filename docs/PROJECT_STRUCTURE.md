# Project Structure

[简体中文](PROJECT_STRUCTURE_ZH.md)

This repository is a board-specific collection of examples and firmware for
ESP32-P4-WIFI6-Touch-LCD-7B.

| Path | Purpose |
| --- | --- |
| `README.md` | Repository overview and quick start |
| `examples/esp-idf/` | Standalone ESP-IDF examples |
| `examples/arduino/` | Arduino sketches and bundled board libraries |
| `firmware/` | Inventoried delivery source and prebuilt factory firmware |
| `firmware/brookesia/` | ESP-Brookesia factory-style firmware project |
| `config/` | Shared ESP32-P4 revision overlays |
| `docs/` | Maintainer and user documentation |
| `.github/` | CI workflows, discovery, and Markdown audit scripts |

## ESP-IDF Projects

Use the lowercase `examples/esp-idf/` path. Linux CI treats case-only paths as
different directories, so do not add another ESP-IDF example root with different
capitalization.

Each project should contain:

- `CMakeLists.txt`
- `main/`
- `sdkconfig.defaults`
- `README.md` for hardware-facing examples

Use the managed 7B BSP component,
`waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.0, for board display, touch, audio,
SD card, and wireless integration. Display applications use the BSP's public
LVGL adapter configuration and locking APIs directly. Keep generated outputs
out of git.

## Arduino Projects

Use `examples/arduino/` for Arduino-ESP32 sketches. Each first-party sketch
lives in `examples/arduino/examples/<name>/` and has exactly one `.ino` file
with the same basename as its directory. `libraries/displays/` carries the
LCD-7B EK79007 DSI, GT911, and backlight configuration; the bundled
`GFX_Library_for_Arduino` and `lvgl` directories provide reproducible display
and LVGL builds. The RS485 and TWAI examples are deliberately standalone and
do not initialize the display.

## Firmware Source

`firmware/brookesia` is an inventoried delivery-source project and does not
enter default example CI. The factory BIN is an immutable delivery artifact.
Firmware-source changes and validation require an explicitly scoped
firmware-maintenance workflow.
