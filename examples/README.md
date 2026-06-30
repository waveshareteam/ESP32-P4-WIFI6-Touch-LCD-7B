# Examples

ESP-IDF examples live under [esp-idf](esp-idf/). Each directory builds as an
independent ESP-IDF project.

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

The examples are organized around this board's 1024 x 600 MIPI-DSI display,
GT911 touch controller, ESP32-C6 wireless path, SD card, audio codec, USB, and
field-bus interfaces.

The numeric prefixes are unique. Existing board-specific example numbers are
kept stable; imported generic examples use the next available numbers.

## Recommended Order

1. `00_board_check`: verify toolchain, flashing, serial monitor, flash, PSRAM,
   and chip revision.
2. `02_hello_world`, `15_nvs_counter`, `16_freertos_tasks`: learn the basic
   ESP-IDF runtime patterns without external peripherals.
3. `03_i2c_tools`, `07_color_panel`, `08_lvgl_display_panel`: bring up display
   and touch hardware.
4. `09_lvgl_demo_v8`, `10_lvgl_demo_v9`, `11_esp_brookesia_phone`: start UI
   application development.
5. Peripheral examples such as SD card, Wi-Fi, audio, USB extended display,
   MP4 playback, RS485, and TWAI should be used when the matching hardware path
   is connected.

For task-based guidance, see [../docs/EXAMPLES_GUIDE.md](../docs/EXAMPLES_GUIDE.md).

## Example Index

| Directory | Purpose | Hardware notes |
| --- | --- | --- |
| [00_board_check](esp-idf/00_board_check/) | First-run board and toolchain check | USB only |
| [01_how_to_create_project](esp-idf/01_how_to_create_project/) | Minimal project template | USB only |
| [02_hello_world](esp-idf/02_hello_world/) | Basic app and logging | USB only |
| [03_i2c_tools](esp-idf/03_i2c_tools/) | I2C scanning and tools | Useful for touch and peripheral bring-up |
| [04_sdmmc](esp-idf/04_sdmmc/) | SD card | Requires SD card |
| [05_wifistation](esp-idf/05_wifistation/) | Wi-Fi station | Uses ESP32-C6 hosted Wi-Fi path |
| [06_i2s_codec](esp-idf/06_i2s_codec/) | I2S audio codec | Uses board audio codec |
| [07_color_panel](esp-idf/07_color_panel/) | EK79007 color bar | 1024 x 600 MIPI-DSI panel |
| [08_lvgl_display_panel](esp-idf/08_lvgl_display_panel/) | LVGL touch/display panel | 1024 x 600 panel and GT911 touch |
| [09_lvgl_demo_v8](esp-idf/09_lvgl_demo_v8/) | LVGL v8 demo | Board BSP |
| [10_lvgl_demo_v9](esp-idf/10_lvgl_demo_v9/) | LVGL v9 demo | Board BSP |
| [11_esp_brookesia_phone](esp-idf/11_esp_brookesia_phone/) | ESP-Brookesia phone UI | 1024 x 600 stylesheet and camera sizing |
| [12_usb_extend_screen](esp-idf/12_usb_extend_screen/) | USB extended screen | Windows-side driver required |
| [13_rs485_test](esp-idf/13_rs485_test/) | RS485 UART echo | Requires RS485 wiring |
| [14_twai_transmit](esp-idf/14_twai_transmit/) | TWAI transmit | Requires CAN/TWAI wiring |
| [15_nvs_counter](esp-idf/15_nvs_counter/) | Persistent boot counter | USB only |
| [16_freertos_tasks](esp-idf/16_freertos_tasks/) | FreeRTOS tasks and queues | USB only |
| [17_system_monitor](esp-idf/17_system_monitor/) | Serial diagnostics shell | USB only |
| [18_mp4_player](esp-idf/18_mp4_player/) | MP4 or AVI video playback | Requires microSD card and MJPEG video |

Generated outputs (`build/`, `managed_components/`, `dependencies.lock`, and
local `sdkconfig`) are ignored and should not be committed.
