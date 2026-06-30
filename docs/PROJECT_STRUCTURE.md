# Project Structure

This repository is a board-specific collection of examples and firmware for
ESP32-P4-WIFI6-Touch-LCD-7B.

| Path | Purpose |
| --- | --- |
| `README.md` | Repository overview and quick start |
| `examples/ESP-IDF/` | Standalone ESP-IDF examples |
| `Firmware/` | Prebuilt firmware and buildable firmware source |
| `Firmware/brookesia/` | ESP-Brookesia factory-style firmware project |
| `config/` | Shared ESP32-P4 revision overlays |
| `docs/` | Maintainer and user documentation |
| `.github/` | CI workflow and discovery script |

## ESP-IDF Projects

Use the exact `examples/ESP-IDF/` path casing. Linux CI treats case-only paths
as different directories, so do not add a lower-case duplicate.

Each project should contain:

- `CMakeLists.txt`
- `main/`
- `sdkconfig.defaults`
- `README.md` for hardware-facing examples

Use the 7B BSP component, `waveshare/esp32_p4_wifi6_touch_lcd_7b`, for board
display, touch, audio, SD card, and wireless integration. Keep generated
outputs out of git.

## Firmware Source

`Firmware/brookesia` mirrors the local Brookesia example as a firmware project
so the source for the factory-style application can be built and checked in CI.
It intentionally keeps the existing `Firmware/` directory name to avoid
case-only path changes on Windows workstations.
