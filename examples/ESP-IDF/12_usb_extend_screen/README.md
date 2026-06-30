# USB Extended Display

[中文版本](./README_cn.md)

This example makes ESP32-P4-WIFI6-Touch-LCD-7B act as a USB secondary display
for Windows. It uses the board's high-speed USB path, 1024 x 600 MIPI-DSI panel,
GT911 touch controller, and optional USB audio function.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable connected to the board's high-speed USB port.
- Windows 10 or Windows 11 host for the PC-side display driver.

## Build and Flash

```bash
cd examples/ESP-IDF/12_usb_extend_screen
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Windows Driver

Install the Windows IDD driver described in [windows_driver](./windows_driver/README.md).

The default USB IDs are:

| Mode | VID/PID |
| --- | --- |
| Composite display, touch, or audio | `USB\VID_303A&PID_2986` |
| Display-only | `USB\VID_303A&PID_2987` |

## Configuration

Useful menuconfig options:

| Option | Purpose |
| --- | --- |
| `CONFIG_USB_EXTEND_SCREEN_HEIGHT` | Horizontal pixel count sent to the host, default `1024` |
| `CONFIG_USB_EXTEND_SCREEN_WIDTH` | Vertical pixel count sent to the host, default `576` for the transported frame |
| `CONFIG_USB_EXTEND_SCREEN_JPEG_QUALITY` | JPEG quality used for transported frames |
| `CONFIG_USB_EXTEND_SCREEN_MAX_FPS` | Maximum output FPS |
| `CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B` | Maximum frame size received from the PC driver |
| `CONFIG_HID_TOUCH_ENABLE` | Enable USB HID touch reports |
| `CONFIG_UAC_AUDIO_ENABLE` | Enable USB audio |

The physical LCD is 1024 x 600. The transported display frame defaults to
1024 x 576 to match the upstream USB display driver behavior.

Generated files such as `sdkconfig`, `build/`, `managed_components/`, and
`dependencies.lock` are ignored and should not be committed.
