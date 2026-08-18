# I2C 工具

[English](README.md)

用于开发板调试和外设诊断的交互式 I2C 命令行工具。

检查触摸、音频、传感器或扩展硬件时，可使用本示例扫描 I2C 设备并读写寄存器。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。
- 可选：连接到已配置 SDA/SCL 引脚的 I2C 设备。

## 构建和烧录

```bash
cd examples/esp-idf/03_i2c_tools
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## 配置

默认 I2C 引脚在 `main/Kconfig.projbuild` 中配置：

| 信号 | 默认 GPIO |
| --- | --- |
| SDA | GPIO7 |
| SCL | GPIO8 |

如果硬件使用其他 I2C 总线，请运行 `idf.py menuconfig` 并修改
`Example Configuration`。
