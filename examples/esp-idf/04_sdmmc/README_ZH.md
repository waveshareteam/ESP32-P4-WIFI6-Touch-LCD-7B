# SDMMC

[English](README.md)

面向开发板 SDMMC 接口的 SD 卡示例。

示例会挂载存储卡、打印卡信息，并通过 ESP-IDF VFS 层执行基本文件访问。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。
- microSD 卡。

## 构建和烧录

```bash
cd examples/esp-idf/04_sdmmc
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## 配置

ESP32-P4 默认 SDMMC 引脚：

| 信号 | GPIO |
| --- | --- |
| CLK | GPIO43 |
| CMD | GPIO44 |
| D0 | GPIO39 |
| D1 | GPIO40 |
| D2 | GPIO41 |
| D3 | GPIO42 |

可使用 `idf.py menuconfig` 在 1-bit 与 4-bit 总线宽度之间切换，或启用存储卡
格式化选项。
