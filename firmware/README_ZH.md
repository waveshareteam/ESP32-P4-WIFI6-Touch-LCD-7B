# 固件

[English](README.md)

本目录包含 ESP32-P4-WIFI6-Touch-LCD-7B 的出厂固件工件和源码。

有关由 ESP-IDF 示例 CI 构建的包及受保护的 Windows 烧录器，请参阅
[固件与 CI 包](../docs/firmware_ZH.md)。它们不会替代此出厂专用镜像或单独维护的
`brookesia/` 源码工程。

## 目录内容

| 路径 | 用途 |
| --- | --- |
| `ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin` | 已有的预编译出厂专用二进制 |
| `brookesia/` | 可构建的 ESP-Brookesia 固件源码 |

## 出厂二进制完整性

出厂专用二进制是不可变的交付制品，其 SHA-256 为：

```text
3a60bb19b90f04914ac1173d9a63df19eeb3626662c72d7d631028aded00c6df
```

轻量仓库检查只验证该文件身份，不会重建、重新打包，也不据此宣称完成硬件验证。

## Brookesia 源码

`brookesia/` 基于本仓库的 `11_esp_brookesia_phone` 示例，并采用
ESP32-P4 platform 的固件源码结构。该工程针对以下能力进行了适配：

- ESP Component Registry 中的 `waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.0
- 1024 x 600 显示分辨率
- GT911 触摸
- ESP32-C6 Hosted Wi-Fi
- 板载音频、SD 卡、摄像头以及 LVGL/Brookesia 应用流程

其中的 `esp_wifi_remote 0.14.*` 依赖是交付源码保留的 legacy 依赖约定，并非可按常规视为
“最新”的版本；它不同于示例树中的 `esp_wifi_remote 1.2.5` 依赖线。只有在获得相匹配的
C6 slave 固件、源码和构建说明，并完成产品运行时验证后，才应复审或升级；仅 CI 编译不能证明
Hosted 运行时或硬件兼容性。

构建：

```bash
cd firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

该交付源码工程与默认 48 项示例矩阵分开。产品工作流会在 IDF 5.5.5 上恰好构建两个
profile：本地默认的 `rev1_3` 和 `rev3_x`，并使用独立目录。profile 包不兼容，不能
混用。修改不得顺带替换或重新打包预编译出厂镜像；CI 编译也不能证明硬件/HIL 运行行为。

## 媒体资源

`brookesia/spiffs/` 包含音乐和游戏演示使用的示例音频资源。发布衍生产品固件前，
请将这些文件替换为具备适当许可证的客户内容。
