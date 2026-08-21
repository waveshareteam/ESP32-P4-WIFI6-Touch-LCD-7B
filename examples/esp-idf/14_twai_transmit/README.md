# TWAI Transmit

[简体中文](README_ZH.md)

TWAI/CAN transmit example for CAN bus validation.

The example configures ESP-IDF's TWAI driver and transmits through the board's
on-board TJA1051 CAN transceiver.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.
- CAN bus wiring with appropriate termination.
- Another node or analyzer to acknowledge and receive frames.

## Build and Flash

```bash
cd examples/esp-idf/14_twai_transmit
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## Configuration

Default pins are configured in `main/Kconfig.projbuild`:

| Signal | Default GPIO |
| --- | --- |
| TX | GPIO22 |
| RX | GPIO21 |

The defaults route directly to the on-board transceiver. Run `idf.py menuconfig`
only if the hardware routing has been modified.
