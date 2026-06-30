# USB 扩展显示

[English](README.md)

本示例可让 ESP32-P4-WIFI6-Touch-LCD-7B 作为 Windows 的 USB 副屏使用。示例使用本板的
高速 USB、1024 x 600 MIPI-DSI 屏、GT911 触摸，以及可选 USB 音频功能。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 连接到开发板高速 USB 口的 USB 线。
- Windows 10 或 Windows 11 主机，用于安装 PC 侧显示驱动。

## 构建和烧录

```bash
cd examples/ESP-IDF/12_usb_extend_screen
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为你的开发板串口。

## Windows 驱动

请按 [windows_driver](./windows_driver/README_CN.md) 中的说明安装 Windows IDD 驱动。

默认 USB ID 如下：

| 模式 | VID/PID |
| --- | --- |
| 复合显示、触摸或音频 | `USB\VID_303A&PID_2986` |
| 仅显示 | `USB\VID_303A&PID_2987` |

## 配置项

常用 menuconfig 选项：

| 选项 | 作用 |
| --- | --- |
| `CONFIG_USB_EXTEND_SCREEN_HEIGHT` | 上报给主机的横向像素数，默认 `1024` |
| `CONFIG_USB_EXTEND_SCREEN_WIDTH` | 上报给主机的纵向像素数，传输帧默认 `576` |
| `CONFIG_USB_EXTEND_SCREEN_JPEG_QUALITY` | 传输帧使用的 JPEG 质量 |
| `CONFIG_USB_EXTEND_SCREEN_MAX_FPS` | 最大输出帧率 |
| `CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B` | PC 驱动传入的单帧最大大小 |
| `CONFIG_HID_TOUCH_ENABLE` | 启用 USB HID 触摸上报 |
| `CONFIG_UAC_AUDIO_ENABLE` | 启用 USB 音频 |

物理 LCD 分辨率是 1024 x 600。传输显示帧默认 1024 x 576，以匹配上游 USB 显示驱动行为。

`sdkconfig`、`build/`、`managed_components/` 和 `dependencies.lock` 等生成文件
会被忽略，不应提交。
