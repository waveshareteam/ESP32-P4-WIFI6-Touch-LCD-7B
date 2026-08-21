# Hello World

[English](README.md)

这是面向 ESP32-P4 的经典 ESP-IDF hello world 示例。

应用会打印芯片信息、Flash 容量和堆信息，并在短暂倒计时后重启。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。

## 构建和烧录

```bash
cd examples/esp-idf/02_hello_world
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

请将 `PORT` 替换为开发板对应的串口。

## 预期输出

串口监视器应输出 `Hello world!`、ESP32-P4 芯片信息、Flash 容量、堆大小和重启倒计时。
