# TWAI 发送

[English](README.md)

用于验证 CAN 总线的 TWAI/CAN 发送示例。

示例配置 ESP-IDF TWAI 驱动，并通过开发板板载 TJA1051 CAN 收发器发送帧。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。
- 带适当终端的 CAN 总线接线。
- 用于确认和接收帧的另一个节点或分析仪。

## 构建和烧录

```bash
cd examples/esp-idf/14_twai_transmit
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## 配置

默认引脚在 `main/Kconfig.projbuild` 中配置：

| 信号 | 默认 GPIO |
| --- | --- |
| TX | GPIO22 |
| RX | GPIO21 |

默认引脚直接连接板载收发器。只有硬件走线已修改时，才需运行 `idf.py menuconfig`
并修改 `Example Configuration`。
