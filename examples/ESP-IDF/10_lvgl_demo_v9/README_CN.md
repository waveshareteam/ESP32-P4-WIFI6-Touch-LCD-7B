# LVGL Demo v9

[English Version](./README.md)

本示例在 ESP32-P4-WIFI6-Touch-LCD-7B 上运行 LVGL v9 widgets demo。示例使用本板
BSP，适配 1024 x 600 EK79007 MIPI-DSI 屏和 GT911 触摸。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于供电、烧录和串口监视器的 USB 线。

## 构建和烧录

```bash
cd examples/ESP-IDF/10_lvgl_demo_v9
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为你的开发板串口。

## 说明

- 示例依赖 `lvgl/lvgl` v9.2.x。
- `main/main.c` 默认启用 `lv_demo_widgets()`，可按需切换到其他 LVGL demo。
- 新 LVGL 应用建议优先从本示例开始。

`sdkconfig`、`build/`、`managed_components/` 和 `dependencies.lock` 等生成文件
会被忽略，不应提交。
