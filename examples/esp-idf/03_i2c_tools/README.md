# I2C Tools

[简体中文](README_ZH.md)

Interactive I2C command-line tools for board bring-up and peripheral debugging.

Use this example to scan I2C devices and read or write registers while checking
touch, audio, sensors, or expansion hardware.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.
- Optional I2C device connected to the configured SDA/SCL pins.

## Build and Flash

```bash
cd examples/esp-idf/03_i2c_tools
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## Configuration

The default I2C pins are configured in `main/Kconfig.projbuild`:

| Signal | Default GPIO |
| --- | --- |
| SDA | GPIO7 |
| SCL | GPIO8 |

Run `idf.py menuconfig` and edit `Example Configuration` if your hardware uses a
different I2C bus.
