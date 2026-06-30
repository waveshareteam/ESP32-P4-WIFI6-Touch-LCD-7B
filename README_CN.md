# ESP32-P4-WIFI6-Touch-LCD-7B

[English](README.md)

本仓库为 Waveshare ESP32-P4-WIFI6-Touch-LCD-7B 开发板提供可构建的
ESP-IDF 示例、CI 自检配置和出厂固件源码。

仓库结构在适合的地方参考
[waveshareteam/ESP32-P4-platform](https://github.com/waveshareteam/ESP32-P4-platform)，
同时保留本 7 英寸 1024 x 600 产品专用的 BSP、显示、触摸、音频、无线和固件配置。

## 硬件概况

| 项目 | 说明 |
| --- | --- |
| MCU | ESP32-P4 |
| 无线协处理器 | ESP32-C6，通过 ESP-Hosted 连接 |
| LCD | 7 英寸 MIPI-DSI EK79007 屏 |
| 分辨率 | 1024 x 600 |
| 触摸 | GT911 |
| BSP 组件 | `waveshare/esp32_p4_wifi6_touch_lcd_7b` |
| ESP-IDF target | `esp32p4` |

## 快速开始

请先安装 ESP-IDF v5.5.4，然后构建板级检查示例：

```bash
cd examples/ESP-IDF/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

板级检查通过后，可以继续尝试显示示例：

```bash
cd examples/ESP-IDF/10_lvgl_demo_v9
idf.py set-target esp32p4
idf.py build
```

构建接近出厂固件形态的 Brookesia 源码工程：

```bash
cd Firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

请将 `PORT` 替换为开发板对应的串口。

## 仓库结构

```text
.
|-- examples/ESP-IDF/   独立 ESP-IDF 示例
|-- Firmware/
|   |-- brookesia/      ESP-Brookesia 出厂固件源码
|   `-- *.bin           已有出厂预编译固件
|-- config/             共享 ESP32-P4 revision 配置覆盖
|-- docs/               构建、CI 和结构说明
`-- .github/            ESP-IDF 构建工作流和检查脚本
```

请使用准确的 `examples/ESP-IDF/` 路径大小写。仓库有意只保留一个 ESP-IDF
示例目录，确保 Linux CI 和 Windows 工作站看到一致的项目结构。

`build/`、`managed_components/`、`dependencies.lock` 和本地 `sdkconfig`
等 ESP-IDF 生成文件会被忽略，不应提交。

## 文档

| 文档 | 用途 |
| --- | --- |
| [快速入门](docs/GETTING_STARTED.md) | 构建、烧录和监视器流程 |
| [示例索引](examples/README.md) | ESP-IDF 示例列表 |
| [示例指南](docs/EXAMPLES_GUIDE.md) | 按客户任务推荐示例使用顺序 |
| [固件说明](Firmware/README.md) | 出厂固件源码和预编译二进制说明 |
| [持续集成](docs/CI.md) | CI 矩阵和本地自检 |
| [项目结构](docs/PROJECT_STRUCTURE.md) | 仓库组织和贡献要求 |
| [ESP32-P4 Revision 配置](docs/ESP32P4_REVISION_CONFIG.md) | 共享芯片 revision 配置覆盖 |

## CI 覆盖

GitHub Actions 工作流会使用 ESP-IDF `v5.5.4` 构建发生变更的 ESP-IDF
工程。依赖已兼容 v6 的工程也会使用 `v6.0.1` 进行检查。

## 出厂固件

原始出厂预编译固件保留在 `Firmware/` 目录下。`Firmware/brookesia`
包含基于本地 ESP-Brookesia 示例和 ESP32-P4-platform 固件结构适配的源码工程。
该工程使用 7B BSP 以及 `BSP_LCD_H_RES/BSP_LCD_V_RES`，因此显示、触摸、
摄像头缓冲区和 UI 布局会跟随 1024 x 600 面板，而不是上游 800 x 1280
平台默认值。

## 许可证

本仓库基于 Apache License 2.0 授权。详情请见 [LICENSE](LICENSE)。
