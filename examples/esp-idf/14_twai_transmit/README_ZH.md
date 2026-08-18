# TWAI 发送

[English](README.md)

用于验证外部 CAN 总线的 TWAI/CAN 发送示例。

示例配置 ESP-IDF TWAI 驱动，并通过指定的 TX/RX GPIO 发送帧。必须连接外部 CAN
收发器。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。
- 外部 CAN/TWAI 收发器。
- CAN 总线接线，以及用于接收帧的另一个节点或分析仪。

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

如果收发器连接到其他引脚，请运行 `idf.py menuconfig` 并修改
`Example Configuration`。
