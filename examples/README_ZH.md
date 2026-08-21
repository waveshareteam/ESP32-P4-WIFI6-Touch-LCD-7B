# 示例

[English](README.md)

本仓库同时提供适用于 ESP32-P4-WIFI6-Touch-LCD-7B 的 [ESP-IDF](esp-idf/)
和 [Arduino](arduino/) 示例。

## ESP-IDF 示例

ESP-IDF 示例位于 [esp-idf](esp-idf/) 下，每个目录都可作为独立 ESP-IDF 工程构建。

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

这些示例围绕本开发板的 1024 x 600 MIPI-DSI 显示屏、GT911 触摸、ESP32-C6
无线链路、SD 卡、音频 codec、USB 和现场总线接口组织。

数字前缀保持唯一。现有板级示例编号保持稳定；导入的通用示例使用下一个可用编号。

### 推荐顺序

1. `00_board_check`：检查工具链、烧录、串口监视器、Flash、PSRAM 和芯片版本。
2. `02_hello_world`、`15_nvs_counter`、`16_freertos_tasks`：在不连接外设的情况下学习
   ESP-IDF 基础运行模式。
3. `03_i2c_tools`、`07_color_panel`、`08_lvgl_display_panel`：调试显示与触摸硬件。
4. `09_lvgl_demo_v9`、`11_esp_brookesia_phone`：开始 UI 应用开发。
5. 仅在连接对应硬件链路后使用 SD 卡、Wi-Fi、音频、USB 扩展显示、MP4 播放、
   RS485 和 TWAI 等外设示例。

### ESP-IDF 索引

| 目录 | 用途 | 硬件说明 |
| --- | --- | --- |
| [00_board_check](esp-idf/00_board_check/) | 首次开发板和工具链检查 | 仅 USB |
| [01_how_to_create_project](esp-idf/01_how_to_create_project/) | 最小工程模板 | 仅 USB |
| [02_hello_world](esp-idf/02_hello_world/) | 基础应用与日志 | 仅 USB |
| [03_i2c_tools](esp-idf/03_i2c_tools/) | I2C 扫描与工具 | 用于触摸和外设调试 |
| [04_sdmmc](esp-idf/04_sdmmc/) | SD 卡 | 需要 SD 卡 |
| [05_wifistation](esp-idf/05_wifistation/) | Wi-Fi station | 使用 ESP32-C6 Hosted Wi-Fi 链路 |
| [06_i2s_codec](esp-idf/06_i2s_codec/) | I2S 音频 codec | 使用板载音频 codec |
| [07_color_panel](esp-idf/07_color_panel/) | EK79007 彩条 | 1024 x 600 MIPI-DSI 屏 |
| [08_lvgl_display_panel](esp-idf/08_lvgl_display_panel/) | LVGL 触摸与显示 | 1024 x 600 屏和 GT911 触摸 |
| [09_lvgl_demo_v9](esp-idf/09_lvgl_demo_v9/) | LVGL v9 演示 | 开发板 BSP |
| [11_esp_brookesia_phone](esp-idf/11_esp_brookesia_phone/) | ESP-Brookesia 手机 UI | 1024 x 600 样式表和摄像头尺寸 |
| [12_usb_extend_screen](esp-idf/12_usb_extend_screen/) | USB 扩展屏 | 需要 Windows 侧驱动 |
| [13_rs485_test](esp-idf/13_rs485_test/) | RS485 UART echo | 需要 RS485 接线 |
| [14_twai_transmit](esp-idf/14_twai_transmit/) | TWAI 发送 | 需要 CAN/TWAI 接线 |
| [15_nvs_counter](esp-idf/15_nvs_counter/) | 持久化启动计数器 | 仅 USB |
| [16_freertos_tasks](esp-idf/16_freertos_tasks/) | FreeRTOS 任务与队列 | 仅 USB |
| [17_system_monitor](esp-idf/17_system_monitor/) | 串口诊断 shell | 仅 USB |
| [18_mp4_player](esp-idf/18_mp4_player/) | MP4 或 AVI 视频播放 | 需要 microSD 卡和 MJPEG 视频 |

## Arduino 示例

[Arduino 示例](arduino/) 使用 Arduino-ESP32 `3.3.11` 和 `ESP32P4 Dev Module`
开发板配置，提供适配 7B 的显示、触摸、摄像头、SD 卡、音频、Wi-Fi、RS485 和
CAN/TWAI 草图。请参阅 [Arduino README](arduino/README_ZH.md) 获取开发板菜单设置
和现场总线接线要求。

按任务选择 ESP-IDF 示例时，请参阅[示例指南](../docs/EXAMPLES_GUIDE_ZH.md)。

`build/`、`managed_components/`、`dependencies.lock` 和本地 `sdkconfig` 等生成输出
已被忽略，不应提交。
