# ESP32-P4-WIFI6-Touch-LCD-7B

Buildable ESP-IDF examples, CI checks, and factory firmware source for the
Waveshare ESP32-P4-WIFI6-Touch-LCD-7B board.

This repository follows the layout used by
[waveshareteam/ESP32-P4-platform](https://github.com/waveshareteam/ESP32-P4-platform)
where it makes sense, while keeping the BSP, display, touch, audio, wireless,
and firmware settings specific to this 7-inch 1024 x 600 product.

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

## Quick Start

Install ESP-IDF v5.5.4 first. Then build the board check example:

```bash
cd examples/ESP-IDF/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

After the board check passes, try a display example:

```bash
cd examples/ESP-IDF/10_lvgl_demo_v9
idf.py set-target esp32p4
idf.py build
```

For the factory-style Brookesia firmware source:

```bash
cd Firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

Replace `PORT` with your board's serial port.

## Repository Layout

```text
.
|-- examples/ESP-IDF/   Standalone ESP-IDF examples
|-- Firmware/
|   |-- brookesia/      ESP-Brookesia factory firmware source
|   `-- *.bin           Existing prebuilt factory firmware
|-- config/             Shared ESP32-P4 revision config overlays
|-- docs/               Build, CI, and structure notes
`-- .github/            ESP-IDF build workflow and checks
```

Use the exact `examples/ESP-IDF/` path casing. The repository intentionally
keeps a single ESP-IDF example tree so Linux CI and Windows workstations see the
same project structure.

Generated ESP-IDF outputs such as `build/`, `managed_components/`,
`dependencies.lock`, and local `sdkconfig` files are intentionally ignored.

## Documentation

| Document | Purpose |
| --- | --- |
| [Getting Started](docs/GETTING_STARTED.md) | Build, flash, and monitor workflow |
| [Examples](examples/README.md) | ESP-IDF example index |
| [Example Guide](docs/EXAMPLES_GUIDE.md) | Recommended example order by customer task |
| [Firmware](Firmware/README.md) | Factory firmware source and prebuilt binary notes |
| [Continuous Integration](docs/CI.md) | CI matrix and local self-checks |
| [Project Structure](docs/PROJECT_STRUCTURE.md) | Repository organization and contribution expectations |
| [ESP32-P4 Revision Config](docs/ESP32P4_REVISION_CONFIG.md) | Shared chip revision overlays |

## CI Coverage

The GitHub Actions workflow builds changed ESP-IDF projects with ESP-IDF
`v5.5.4`. Projects whose dependencies are already v6-ready are also checked with
`v6.0.1`.

## Factory Firmware

The original prebuilt factory binary is retained under `Firmware/`.
`Firmware/brookesia` contains a source project adapted from the local
ESP-Brookesia example and the ESP32-P4-platform firmware layout. It uses the
7B BSP and `BSP_LCD_H_RES/BSP_LCD_V_RES`, so display, touch, camera buffers,
and UI layout follow the 1024 x 600 panel instead of upstream 800 x 1280
platform defaults.

## License

This repository is licensed under the Apache License 2.0. See [LICENSE](LICENSE).
