# 示例指南

[English](EXAMPLES_GUIDE.md)

可按客户任务使用本指南选择示例。

## 首次上电

建议先运行 `examples/esp-idf/00_board_check`。它只需要 USB，即可确认 ESP-IDF
目标芯片、Flash、PSRAM、芯片版本、烧录流程和串口监视器是否正常。

随后可运行：

- `examples/esp-idf/02_hello_world`：最小 ESP-IDF 应用。
- `examples/esp-idf/15_nvs_counter`：持久化设置。
- `examples/esp-idf/16_freertos_tasks`：基础任务和队列模式。

## 显示与触摸

验证 7 英寸 1024 x 600 显示屏和 GT911 触摸时，可使用：

- `examples/esp-idf/07_color_panel`
- `examples/esp-idf/08_lvgl_display_panel`
- `examples/esp-idf/09_lvgl_demo_v9`

新的显示代码应使用 `waveshare/esp32_p4_wifi6_touch_lcd_7b` 中的
`BSP_LCD_H_RES` 和 `BSP_LCD_V_RES`。

## UI 与固件

完整的 ESP-Brookesia UI 示例位于 `examples/esp-idf/11_esp_brookesia_phone`。
如需接近出厂应用的固件源码工程，请使用 `firmware/brookesia`。

Brookesia 工程已适配 7B 的 1024 x 600 屏。发布定制产品固件前，请替换示例媒体
资源和面向客户的字符串。

## 外设

| 任务 | 示例 |
| --- | --- |
| I2C 扫描与调试 | `examples/esp-idf/03_i2c_tools` |
| SD 卡 | `examples/esp-idf/04_sdmmc` |
| Hosted Wi-Fi | `examples/esp-idf/05_wifistation` |
| 音频 codec | `examples/esp-idf/06_i2s_codec` |
| MP4 或 AVI 播放 | `examples/esp-idf/18_mp4_player` |
| USB 扩展显示 | `examples/esp-idf/12_usb_extend_screen` |
| RS485 | `examples/esp-idf/13_rs485_test` |
| CAN/TWAI | `examples/esp-idf/14_twai_transmit` |

外设示例需要连接与任务对应的硬件链路。
