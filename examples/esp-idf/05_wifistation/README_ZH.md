# Wi-Fi Station

[English](README.md)

使用开发板 ESP32-C6 Hosted Wi-Fi 链路的 Wi-Fi station 示例。

应用会连接到 menuconfig 中配置的接入点，并打印获得的 IP 地址。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。
- 2.4 GHz Wi-Fi 接入点。

## 配置 Wi-Fi

```bash
cd examples/esp-idf/05_wifistation
idf.py set-target esp32p4
idf.py menuconfig
```

设置 `Example Configuration` > `WiFi SSID` 和 `WiFi Password`。

## 构建和烧录

```bash
idf.py build
idf.py -p PORT flash monitor
```

请将 `PORT` 替换为开发板对应的串口。

CI 使用 ESP-IDF v5.5.5 和 v6.0.2 编译本示例。C6/ESP-Hosted 运行边界仍然存在：
此兼容固定和编译垫片仅保留源码覆盖，尚未验证现有协处理器固件或硬件运行时。
