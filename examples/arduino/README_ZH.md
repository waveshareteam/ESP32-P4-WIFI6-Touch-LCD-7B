# Arduino 示例

[English](README.md)

本目录提供适用于 Waveshare ESP32-P4-WIFI6-Touch-LCD-7B 的 Arduino 草图和随仓库
库文件。开发板使用 1024 x 600 MIPI-DSI IPS 屏、GT911 电容触摸，并支持选配
OV5647 MIPI-CSI 摄像头。

## 开发板设置（Arduino IDE）

- Arduino-ESP32 core `3.3.11`。
- 开发板：`ESP32P4 Dev Module`（`esp32:esp32:esp32p4`）。
- 菜单选项：
  - `Chip Variant`：rev1.3 硬件选 `Before v3.00`，rev3.x 硬件选 `After v3.00`。
  - `PSRAM`：`Enabled`
  - `Flash Size`：`32 MB`
  - `Flash Mode`：`QIO`
  - `Flash Frequency`：`80 MHz`
  - `Partition Scheme`：`13M APP / 7M data (32 MB)`
  - `Upload Mode`：`Default (USB-UART bridge)`
- 显示、摄像头和 LVGL 草图均要求启用 PSRAM。

## 示例

| 草图 | 说明 |
| --- | --- |
| `01_HelloWorld` | Arduino_GFX EK79007 DSI 显示最小点亮 |
| `02_AsciiTable` | Arduino_GFX 字符表和功能演示 |
| `03_Drawing_board` | GT911 五点电容触摸画板 |
| `04_LVGLV9_Arduino` | LVGL 9 控件界面和触摸输入 |
| `05_GFX_ESPWiFiAnalyzer` | 通过 ESP32-C6 协处理器进行图形化 Wi-Fi 扫描 |
| `06_Camera_Preview` | OV5647 MIPI-CSI 摄像头实时上屏 |
| `07_Camera_ISP_Tuning` | 实时预览和串口 ISP/3A 调参 |
| `08_SD_Card` | microSD SDIO 卡读写 |
| `09_Audio_Playback` | ES8311 扬声器播放演示 |
| `10_Mic_Record` | ES7210 麦克风采集演示 |
| `11_RS485_Echo` | 板载 RS485 UART 回显 |
| `12_TWAI_Transmit` | 通过外置收发器发送 CAN/TWAI 帧 |

## 现场总线说明

`11_RS485_Echo` 使用 UART1，TX 为 GPIO27，RX 为 GPIO26。草图会在打开串口前
启用板载 LDO VO4 的 3.3 V 供电。RS485 收发器具有自动方向控制，因此草图不使用
RTS 或 DE 引脚。请将 RS485 对端连接到开发板的 `A`、`B` 和地端子，并保持相同
波特率。

`12_TWAI_Transmit` 使用 GPIO22 作为 TX、GPIO21 作为 RX，默认速率为 500 kbit/s。
请将信号连接到外置 CAN 收发器和正确终端匹配的 CAN 总线。ESP32-P4 提供 TWAI
控制器，但不是 CAN 收发器。

## 音频和摄像头说明

`09_Audio_Playback` 通过 TX-only I2S 驱动 ES8311（I2C `0x18`）：MCLK GPIO13、
BCLK GPIO12、LRCK GPIO10、DOUT GPIO9，并通过 GPIO53 使能扬声器。
`10_Mic_Record` 通过 RX-only I2S（DIN GPIO11）以 16 kHz/16-bit 采集
ES7210 麦克风（I2C `0x40`）。

`06_Camera_Preview` 和 `07_Camera_ISP_Tuning` 使用 Arduino-ESP32 自带的
`ESP_Video` 库。请将 OV5647 模组接到 MIPI-CSI 接口；默认 RAW8 帧由 ISP 转换为
RGB565。`07_Camera_ISP_Tuning` 支持串口命令 `g`、`e`、`a`、`v`、`h`、`t` 和 `s`。

## 随仓库库文件

- `displays/`：LCD-7B EK79007 显示、GT911 触摸、I2C 和串口日志支持
- `GFX_Library_for_Arduino`：支持 ESP32-P4 MIPI-DSI 的 Arduino_GFX
- `lvgl` 与 `lv_conf.h`：供 `04_LVGLV9_Arduino` 使用的 LVGL 9

硬件详情请参阅[主 README](../README_ZH.md)和[官方产品文档]
(https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)。
