# ESP-Brookesia Phone

[English](README.md)

本示例在 ESP32-P4-WIFI6-Touch-LCD-7B 上运行 ESP-Brookesia 手机风格 UI，使用
LVGL 9 版本的 Brookesia core，并在启动器中只保留一个简单的 Squareline 示例 app。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于供电、烧录和串口监视器的 USB 线。

## 构建和烧录

```bash
cd examples/esp-idf/11_esp_brookesia_phone
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为开发板串口。

## 板级适配

- 使用 Registry 托管的 `waveshare/esp32_p4_wifi6_touch_lcd_7b` BSP `3.0.1`。
- 使用 BSP 提供的显示配置和加锁 API，并保留 7B 屏幕旋转及触摸方向配置。
- 使用仓库内 `components/brookesia_core` 中的 LVGL 9 Brookesia core。
- 通过 app registry 启动 Phone 系统，只安装 `brookesia_app_squareline_demo` 这一个示例 app。
- 使用 1024 x 600 样式表。

原有的摄像头、音频播放器、视频播放器、Hosted Wi-Fi 和媒体资源 app 已从本示例移除。
对应硬件功能仍可在专用 examples 和 firmware 工程中使用。

## CI 兼容性

本示例面向 ESP-IDF 5.5 及以上的 BSP 合同。Brookesia core 固定使用 LVGL `9.5.0`，
core 和示例 app 均保存在仓库内，不再依赖旧的 ESP-Brookesia LVGL 8 软件包。

GitHub Actions 只验证编译；显示和触摸的运行时行为仍需使用真实 7B 开发板验证。

`sdkconfig`、`build/`、`managed_components/` 和 `dependencies.lock` 等生成文件会被忽略，
不应提交。
