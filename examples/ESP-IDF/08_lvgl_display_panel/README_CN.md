# LVGL 显示触摸面板

[English](README.md)

本示例使用一个小型 LVGL app 验证 1024 x 600 显示屏和 GT911 触摸。程序会依次显示红、
绿、蓝纯色屏，然后切换到白色触摸画布；触摸面板时会在触摸坐标绘制黑色方块。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于供电、烧录和串口监视器的 USB 线。

## 构建和烧录

```bash
cd examples/ESP-IDF/08_lvgl_display_panel
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为你的开发板串口。

## 预期现象

LCD 会依次显示红、绿、蓝，然后显示白屏。触摸面板时，应在触摸坐标处出现黑色方块。

可在进入更完整的 LVGL demo 前，用本示例检查显示颜色、触摸坐标映射和 LVGL 输入处理。
