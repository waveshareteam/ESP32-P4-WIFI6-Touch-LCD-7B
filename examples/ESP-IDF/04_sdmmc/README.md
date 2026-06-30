# SDMMC

SD card example for the board SDMMC interface.

The example mounts a card, prints card information, and exercises basic file
access through ESP-IDF's VFS layer.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.
- microSD card.

## Build and Flash

```bash
cd examples/ESP-IDF/04_sdmmc
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## Configuration

Default ESP32-P4 SDMMC pins:

| Signal | GPIO |
| --- | --- |
| CLK | GPIO43 |
| CMD | GPIO44 |
| D0 | GPIO39 |
| D1 | GPIO40 |
| D2 | GPIO41 |
| D3 | GPIO42 |

Use `idf.py menuconfig` to switch between 1-bit and 4-bit bus width or enable
card formatting options.
