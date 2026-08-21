# 快速开始

[English](GETTING_STARTED.md)

本开发板使用 ESP-IDF 目标 `esp32p4` 和
ESP Component Registry 中的
[`waveshare/esp32_p4_wifi6_touch_lcd_7b` BSP](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_7b/versions/3.0.1)
3.0.1。该版本通过 `esp_lvgl_adapter` 0.6.x 集成 LVGL；不要再并列添加旧的
`esp_lvgl_port`。

## 推荐检查顺序

1. 构建开发板检查示例。
2. 构建一个显示示例。
3. 构建 Brookesia 固件源码。

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
```

```bash
cd examples/esp-idf/09_lvgl_demo_v9
idf.py set-target esp32p4
idf.py build
```

```bash
cd firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

使用以下命令烧录并打开串口监视器：

```bash
idf.py -p PORT flash monitor
```

请将 `PORT` 替换为开发板对应的串口。

## 显示说明

7B 硬件分辨率为 1024 x 600。不要在本开发板上复用 ESP32-P4 平台中面向
800 x 1280 或 10.1 英寸屏的 BSP 默认值。新的显示、触摸、摄像头和视频代码应使用
`BSP_LCD_H_RES` 与 `BSP_LCD_V_RES`。
