# Examples

ESP-IDF examples live under [esp-idf](esp-idf/). Each directory is intended to
build as an independent ESP-IDF project.

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

The examples are organized around this board's 1024 x 600 MIPI-DSI display,
GT911 touch controller, ESP32-C6 wireless path, SD card, audio codec, USB, and
field-bus interfaces. Several small examples were brought over from
ESP32-P4-platform with little or no code change because they only depend on the
ESP32-P4 target and standard ESP-IDF services.

| Directory | Purpose | Notes |
| --- | --- | --- |
| [00_board_check](esp-idf/00_board_check/) | First-run board and toolchain check | Imported from ESP32-P4-platform |
| [01_HowToCreateProject](esp-idf/01_HowToCreateProject/) | Minimal project template | Local starter project |
| [02_HelloWorld](esp-idf/02_HelloWorld/) | Basic app and logging | Standard ESP-IDF check |
| [03_i2c_tools](esp-idf/03_i2c_tools/) | I2C scanning and tools | Useful for touch and peripheral bring-up |
| [03_nvs_counter](esp-idf/03_nvs_counter/) | Persistent boot counter | Imported from ESP32-P4-platform |
| [04_freertos_tasks](esp-idf/04_freertos_tasks/) | FreeRTOS tasks and queues | Imported from ESP32-P4-platform |
| [04_sdmmc](esp-idf/04_sdmmc/) | SD card | Uses board SDMMC wiring |
| [05_wifistation](esp-idf/05_wifistation/) | Wi-Fi station | Uses ESP32-C6 hosted Wi-Fi path |
| [06_I2SCodec](esp-idf/06_I2SCodec/) | I2S audio codec | Includes audio sample data |
| [07_color_panel](esp-idf/07_color_panel/) | EK79007 color bar | 1024 x 600 MIPI-DSI bring-up |
| [08_lvgl_display_panel](esp-idf/08_lvgl_display_panel/) | LVGL touch/display panel | Touch bounds use BSP resolution |
| [09_lvgl_demo_v8](esp-idf/09_lvgl_demo_v8/) | LVGL v8 demo | Board BSP |
| [10_lvgl_demo_v9](esp-idf/10_lvgl_demo_v9/) | LVGL v9 demo | Board BSP |
| [11_esp_brookesia_phone](esp-idf/11_esp_brookesia_phone/) | ESP-Brookesia phone UI | 1024 x 600 stylesheet and camera sizing |
| [12_usb_extend_screen](esp-idf/12_usb_extend_screen/) | USB extended screen | Includes USB and touch components |
| [13_RS485_Test](esp-idf/13_RS485_Test/) | RS485 UART echo | Requires RS485 wiring |
| [14_TWAItransmit](esp-idf/14_TWAItransmit/) | TWAI transmit | Requires CAN/TWAI wiring |
| [19_system_monitor](esp-idf/19_system_monitor/) | Serial diagnostics shell | Imported from ESP32-P4-platform |

Generated outputs (`build/`, `managed_components/`, `dependencies.lock`, and
local `sdkconfig`) are ignored and should not be committed.
