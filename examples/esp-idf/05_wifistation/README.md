# Wi-Fi Station

[简体中文](README_ZH.md)

Wi-Fi station example using the board ESP32-C6 hosted Wi-Fi path.

The app connects to an access point configured through menuconfig and prints the
assigned IP address.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.
- 2.4 GHz Wi-Fi access point.

## Configure Wi-Fi

```bash
cd examples/esp-idf/05_wifistation
idf.py set-target esp32p4
idf.py menuconfig
```

Set `Example Configuration` > `WiFi SSID` and `WiFi Password`.

## Build and Flash

```bash
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

CI compiles this example with ESP-IDF v5.5.5 and v6.0.2. The C6/ESP-Hosted
runtime boundary remains: this compatibility pin and compile shim preserve
source coverage but do not verify the existing coprocessor firmware or hardware
runtime.
