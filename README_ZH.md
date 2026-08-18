<div align="center">
  <h1>ESP32-P4-WIFI6-Touch-LCD-7B</h1>
  <p><strong>基于 ESP32-P4 与 ESP32-C6 的 7 英寸 1024 × 600 触摸屏开发板</strong></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/actions/workflows/esp-idf-examples.yml"><img alt="ESP-IDF 示例" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/actions/workflows/esp-idf-examples.yml/badge.svg"></a>
    <a href="LICENSE"><img alt="许可证" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B"></a>
  </p>
  <p>
    <a href="README.md">English</a> ·
    <a href="https://www.waveshare.net/shop/ESP32-P4-WIFI6-Touch-LCD-7B.htm">🌐 产品页</a> ·
    <a href="https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/">📚 中文文档</a> ·
    <a href="examples/esp-idf/">🧩 ESP-IDF 示例</a> ·
    <a href="examples/arduino/">🔧 Arduino 示例</a> ·
    <a href="firmware/">📦 固件</a>
  </p>
  <img src="assets/ESP32-P4-WIFI6-Touch-LCD-7B-details-1.jpg" alt="微雪 ESP32-P4-WIFI6-Touch-LCD-7B" width="720">
</div>

---

## ✨ 产品概述

本仓库为微雪 ESP32-P4-WIFI6-Touch-LCD-7B 提供官方 ESP-IDF 与 Arduino 示例、
GitHub Actions 验证、ESP-Brookesia 固件源码以及预编译出厂固件。

开发板结合了 ESP32-P4 的多媒体处理能力与 ESP32-C6 无线协处理器，并配备
7 英寸电容触摸屏、摄像头和音频接口、USB、microSD 卡槽以及多种工业扩展接口。
完整的购买信息请参阅[商品页](https://www.waveshare.net/shop/ESP32-P4-WIFI6-Touch-LCD-7B.htm)，
硬件和开发说明请参阅[官方中文文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/)。

## 🖥️ 硬件概况

| 功能 | 器件 / 接口 |
| --- | --- |
| 主处理器 | ESP32-P4NRW32，HP 系统双核 RISC-V 主频最高 360 MHz，并包含低功耗单核 |
| 存储 | 封装内 32 MB PSRAM，板载 32 MB NOR Flash |
| 无线 | ESP32-C6-MINI-1，通过 SDIO 提供 2.4 GHz Wi-Fi 6 与 Bluetooth 5 (LE) |
| 显示 | 7 英寸 1024 × 600 IPS 触摸屏，MIPI-DSI 接口 |
| 触摸 | GT911 电容触摸控制器，支持五点触控 |
| 摄像头 | MIPI-CSI（2-lane）接口；摄像头版本可选配 OV5647 |
| 音频 | ES8311 编解码器、ES7210 音频 ADC、双麦克风与扬声器接口 |
| 存储与扩展 | microSD、USB 2.0 OTG HS、CAN/TWAI、RS485、I2C、UART 与 GPIO |
| 板级支持 | 托管组件：`waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.0 |
| ESP-IDF target | `esp32p4` |

> [!NOTE]
> 硬件引脚分配以官方产品文档和板级支持包为准。本仓库目前没有包含本地原理图副本。

## 🚀 快速开始

安装 ESP-IDF `v5.5.5` 后，先构建板级检查示例：

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

请将 `PORT` 替换为开发板对应的串口。板级检查通过后，可继续阅读
[快速入门](docs/GETTING_STARTED_ZH.md)，或从下表选择所需示例。

## 🧪 ESP-IDF 示例

| 示例 | 主要功能 |
| --- | --- |
| [00_board_check](examples/esp-idf/00_board_check/) | 首次运行时检查开发板、存储器和芯片版本 |
| [01_how_to_create_project](examples/esp-idf/01_how_to_create_project/) | 最小工程模板 |
| [02_hello_world](examples/esp-idf/02_hello_world/) | 基础应用与日志输出 |
| [03_i2c_tools](examples/esp-idf/03_i2c_tools/) | I2C 扫描与诊断 |
| [04_sdmmc](examples/esp-idf/04_sdmmc/) | microSD 卡访问 |
| [05_wifistation](examples/esp-idf/05_wifistation/) | 通过 ESP32-C6 的 ESP-Hosted 通道连接 Wi-Fi |
| [06_i2s_codec](examples/esp-idf/06_i2s_codec/) | 板载音频输入与输出 |
| [07_color_panel](examples/esp-idf/07_color_panel/) | MIPI-DSI 彩条点屏 |
| [08_lvgl_display_panel](examples/esp-idf/08_lvgl_display_panel/) | LVGL 显示与 GT911 触摸 |
| [09_lvgl_demo_v9](examples/esp-idf/09_lvgl_demo_v9/) | LVGL 9 演示 |
| [11_esp_brookesia_phone](examples/esp-idf/11_esp_brookesia_phone/) | ESP-Brookesia 应用界面 |
| [12_usb_extend_screen](examples/esp-idf/12_usb_extend_screen/) | USB 扩展屏 |
| [13_rs485_test](examples/esp-idf/13_rs485_test/) | RS485 收发 |
| [14_twai_transmit](examples/esp-idf/14_twai_transmit/) | CAN/TWAI 发送 |
| [15_nvs_counter](examples/esp-idf/15_nvs_counter/) | NVS 持久化计数器 |
| [16_freertos_tasks](examples/esp-idf/16_freertos_tasks/) | FreeRTOS 任务与队列 |
| [17_system_monitor](examples/esp-idf/17_system_monitor/) | 串口系统诊断 |
| [18_mp4_player](examples/esp-idf/18_mp4_player/) | 从 microSD 播放 MP4 或 AVI 视频 |

完整的推荐学习顺序和硬件要求请参阅[示例索引](examples/README_ZH.md)。

## 🧪 Arduino 示例

[`examples/arduino/`](examples/arduino/) 提供 12 个基于 Arduino-ESP32 3.3.11 的
7B 草图，覆盖显示、GT911 触摸、ESP32-C6 Wi-Fi 链路、摄像头、microSD、音频、
RS485 和 CAN/TWAI。请选择 `ESP32P4 Dev Module`、启用 PSRAM，并按 ESP32-P4
硅片版本选择对应的 Chip Variant。菜单设置、引脚映射和现场总线接线要求请参阅
[Arduino 示例指南](examples/arduino/README_ZH.md)。

## 📡 ESP32-P4 与 ESP32-C6 无线连接

ESP32-P4 本身不集成无线射频，Wi-Fi 与蓝牙由 ESP32-C6 协处理器通过 SDIO
和 ESP-Hosted 提供。修改或重新烧录 ESP32-C6 前，请确保其从机固件与无线工程
声明的 `esp_hosted`、`esp_wifi_remote` 组件版本兼容。

## ✅ 持续集成

[ESP-IDF 示例工作流](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/actions/workflows/esp-idf-examples.yml)
会自动发现本仓库维护的工程，并以 `esp32p4` 为目标执行构建：

| ESP-IDF 版本 | 当前覆盖范围 |
| --- | --- |
| `v5.5.5` | 全部 17 个纳入矩阵的一方示例 |
| `v6.0.2` | 全部 17 个纳入矩阵的一方示例 |

[Arduino 示例工作流](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/actions/workflows/arduino-examples.yml)
使用 Arduino-ESP32 3.3.11 和 `prev3` Chip Variant 编译全部 12 个 Arduino 草图。
该工作流仅提供编译覆盖，不发布固件 ZIP。

轻量发现任务会先对完整的 Pull Request 差异进行分类，再决定是否启动耗时构建。
仅文档或治理文件的变更只运行仓库检查；直接源码变更只选择受影响示例，共享 CI
或配置变更则选择全部 17 个示例。44 个示例构建默认使用 pre-v3 的 `rev1_3` 硅片
profile，且不会倍增矩阵。`firmware/brookesia` 不属于该示例矩阵，目前也没有 GitHub
Actions 工作流构建或打包它。完整路由和手动触发选项请参阅
[持续集成说明](docs/CI_ZH.md)。CI 仅提供编译证据，不是硬件/HIL 验证；仓库没有本地原理图，
并继续使用在线 BSP/应用 glue 边界。

## 📦 固件

[`firmware/`](firmware/) 目录包含两类不同用途的固件：

- [`firmware/brookesia/`](firmware/brookesia/) 是交付源码工程，仅作盘点，
  不由默认示例 CI 构建。
- `firmware/ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin` 是已有的预编译
  出厂固件，不是 CI 构建产物。

出厂 BIN 文件是不可变的交付工件。只有在明确限定的固件维护流程中，才会修改和
验证固件源码。交付与烧录边界请参阅[固件说明](firmware/README_ZH.md)。

## 🗂️ 仓库结构

| 路径 | 用途 |
| --- | --- |
| [`examples/esp-idf/`](examples/esp-idf/) | 本仓库维护的 ESP-IDF 工程 |
| [`examples/arduino/`](examples/arduino/) | Arduino 草图和随仓库板级库 |
| [`firmware/`](firmware/) | Brookesia 源码与预编译出厂固件 |
| [`config/`](config/) | 共享 ESP32-P4 芯片版本配置 |
| [`docs/`](docs/) | 快速入门、CI、结构与芯片版本说明 |
| [`assets/`](assets/) | 仓库文档使用的产品图片 |
| [`.github/`](.github/) | CI 工作流与仓库检查 |

请使用小写的 `examples/esp-idf/` 和 `examples/arduino/` 路径。`build/`、
`managed_components/`、`dependencies.lock` 和本地 `sdkconfig` 等生成文件会被忽略，
不应提交。

## 📚 文档

- [官方中文产品文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/)
- [快速入门](docs/GETTING_STARTED_ZH.md)
- [示例索引](examples/README_ZH.md)
- [Arduino 示例](examples/arduino/README_ZH.md)
- [示例指南](docs/EXAMPLES_GUIDE_ZH.md)
- [固件说明](firmware/README_ZH.md)
- [持续集成](docs/CI_ZH.md)
- [项目结构](docs/PROJECT_STRUCTURE_ZH.md)
- [ESP32-P4 芯片版本配置](docs/ESP32P4_REVISION_CONFIG_ZH.md)

## 🤝 支持与贡献

欢迎提交贡献和可复现的问题报告。请附上开发板版本、示例路径、ESP-IDF 版本、
复现步骤、预期行为、实际行为以及相关串口日志。

- [贡献指南](CONTRIBUTING_ZH.md)
- [支持指南](SUPPORT_ZH.md)
- [提交 Issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/issues/new)
- [技术支持](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/Technical-Support/)

## 📄 许可证

本仓库基于 Apache License 2.0 授权。详情请见 [LICENSE](LICENSE)。
