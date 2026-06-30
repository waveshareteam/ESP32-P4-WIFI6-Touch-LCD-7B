# ESP32-P4-WIFI6-Touch-LCD-7B

ESP-IDF examples, CI checks, and factory firmware sources for the Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B board.

This repository is aligned with the structure used by
[waveshareteam/ESP32-P4-platform](https://github.com/waveshareteam/ESP32-P4-platform),
while keeping the board-specific BSP and display configuration for this 7-inch
1024 x 600 product.

## Hardware Profile

| Item | Value |
| --- | --- |
| MCU | ESP32-P4 |
| Wireless coprocessor | ESP32-C6 over ESP-Hosted |
| LCD | 7-inch MIPI-DSI EK79007 panel |
| Resolution | 1024 x 600 |
| Touch | GT911 |
| BSP component | `waveshare/esp32_p4_wifi6_touch_lcd_7b` |
| ESP-IDF target | `esp32p4` |

## Repository Layout

```text
.
|-- examples/esp-idf/   Standalone ESP-IDF examples
|-- Firmware/
|   |-- brookesia/      ESP-Brookesia factory firmware source
|   `-- *.bin           Existing prebuilt factory firmware
|-- config/             Shared ESP32-P4 revision config overlays
|-- docs/               Build, CI, and structure notes
`-- .github/            ESP-IDF build workflow
```

Generated ESP-IDF outputs such as `build/`, `managed_components/`,
`dependencies.lock`, and local `sdkconfig` files are intentionally ignored.

## Quick Start

Install ESP-IDF v5.5.4 or a compatible v6.x release, then build an example:

```bash
cd examples/esp-idf/10_lvgl_demo_v9
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

For the factory-style Brookesia firmware source:

```bash
cd Firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

The GitHub Actions workflow builds changed ESP-IDF examples and the firmware
source with both ESP-IDF `v5.5.4` and `v6.0.1`.

## Documentation

| Document | Purpose |
| --- | --- |
| [Examples](examples/README.md) | ESP-IDF example index and notes |
| [Getting Started](docs/GETTING_STARTED.md) | Build, flash, and monitor workflow |
| [Firmware](Firmware/README.md) | Factory firmware source and prebuilt binary notes |
| [Continuous Integration](docs/CI.md) | CI matrix and local self-checks |
| [Project Structure](docs/PROJECT_STRUCTURE.md) | Repository organization and contribution expectations |
| [ESP32-P4 Revision Config](docs/ESP32P4_REVISION_CONFIG.md) | Shared chip revision overlays |

## Factory Firmware

The original prebuilt factory binary is retained under `Firmware/`.
`Firmware/brookesia` contains a source project adapted from the local
ESP-Brookesia example and the ESP32-P4-platform firmware layout. It uses the
7B BSP and `BSP_LCD_H_RES/BSP_LCD_V_RES`, so display, touch, camera buffers,
and UI layout follow the 1024 x 600 panel instead of the upstream 800 x 1280
platform defaults.

## License

This repository is licensed under the Apache License 2.0. See [LICENSE](LICENSE).
