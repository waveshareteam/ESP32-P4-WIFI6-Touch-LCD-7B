# TWAI Transmit

[简体中文](README_ZH.md)

TWAI/CAN transmit example for external CAN bus validation.

The example configures ESP-IDF's TWAI driver and transmits frames through the
configured TX/RX GPIOs. An external CAN transceiver is required.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.
- External CAN/TWAI transceiver.
- CAN bus wiring and another node or analyzer to receive frames.

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

Run `idf.py menuconfig` and edit `Example Configuration` if your transceiver is
wired to different pins.
