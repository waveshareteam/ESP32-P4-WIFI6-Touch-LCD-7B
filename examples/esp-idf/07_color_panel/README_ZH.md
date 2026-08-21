# 彩色面板

[English](README.md)

面向 1024 x 600 LCD 的底层 EK79007 MIPI-DSI 彩条测试。

进入 LVGL 之前，可先用本示例验证 LCD 面板驱动。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。

## 构建和烧录

```bash
cd examples/esp-idf/07_color_panel
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## 说明

- 面板分辨率配置为 1024 x 600。
- 示例直接使用 EK79007 MIPI-DSI 面板驱动。
- 通过本底层显示检查后，可使用
  [08_lvgl_display_panel](../08_lvgl_display_panel/) 验证触摸与 LVGL。
