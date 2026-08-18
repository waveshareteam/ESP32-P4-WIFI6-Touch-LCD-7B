# Arduino Examples

[简体中文](README_ZH.md)

Arduino sketches and bundled libraries for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
(1024 x 600 MIPI-DSI IPS display, GT911 capacitive touch, and optional OV5647
MIPI-CSI camera).

## Board settings (Arduino IDE)

- Arduino-ESP32 core `3.3.11`.
- Board: `ESP32P4 Dev Module` (`esp32:esp32:esp32p4`).
- Menu options:
  - `Chip Variant`: `Before v3.00` for rev1.3 hardware or `After v3.00` for rev3.x hardware.
  - `PSRAM`: `Enabled`
  - `Flash Size`: `32 MB`
  - `Flash Mode`: `QIO`
  - `Flash Frequency`: `80 MHz`
  - `Partition Scheme`: `13M APP / 7M data (32 MB)`
  - `Upload Mode`: `Default (USB-UART bridge)`
- Enable PSRAM before building a display, camera, or LVGL sketch.

## Examples

| Sketch | Description |
| --- | --- |
| `01_HelloWorld` | Minimal Arduino_GFX EK79007 DSI display bring-up |
| `02_AsciiTable` | Arduino_GFX capability and character table |
| `03_Drawing_board` | GT911 five-point capacitive touch drawing |
| `04_LVGLV9_Arduino` | LVGL 9 widgets UI with touch input |
| `05_GFX_ESPWiFiAnalyzer` | Graphical Wi-Fi scan through the ESP32-C6 coprocessor |
| `06_Camera_Preview` | OV5647 MIPI-CSI camera preview on the display |
| `07_Camera_ISP_Tuning` | Live preview with serial ISP/3A controls |
| `08_SD_Card` | microSD read/write over the SDIO slot |
| `09_Audio_Playback` | ES8311 speaker playback demo |
| `10_Mic_Record` | ES7210 microphone capture demo |
| `11_RS485_Echo` | Onboard RS485 UART echo |
| `12_TWAI_Transmit` | CAN/TWAI frame transmit through an external transceiver |

## Field-bus notes

`11_RS485_Echo` uses UART1 with TX on GPIO27 and RX on GPIO26. The sketch
enables the board's LDO VO4 3.3 V rail before opening the port. The RS485
transceiver has automatic direction control, so the sketch does not use an RTS
or DE pin. Connect a peer to the board's RS485 `A`, `B`, and ground terminals
at the same baud rate.

`12_TWAI_Transmit` uses TX GPIO22 and RX GPIO21 at 500 kbit/s. Connect those
signals to an external CAN transceiver and a correctly terminated CAN bus. The
ESP32-P4 provides the TWAI controller but is not itself a CAN transceiver.

## Audio and camera notes

`09_Audio_Playback` drives the ES8311 codec (I2C `0x18`) over TX-only I2S
(MCLK GPIO13, BCLK GPIO12, LRCK GPIO10, DOUT GPIO9) and enables the speaker
through GPIO53. `10_Mic_Record` captures the ES7210 microphones (I2C `0x40`)
over RX-only I2S (DIN GPIO11) at 16 kHz/16-bit.

`06_Camera_Preview` and `07_Camera_ISP_Tuning` use the `ESP_Video` library
bundled with Arduino-ESP32. Connect an OV5647 module to the MIPI-CSI connector.
The default sensor mode streams RAW8 frames that the ISP converts to RGB565.
`07_Camera_ISP_Tuning` accepts `g`, `e`, `a`, `v`, `h`, `t`, and `s` commands
through the serial monitor.

## Bundled libraries

- `displays/` - LCD-7B EK79007 display, GT911 touch, I2C, and serial-log support
- `GFX_Library_for_Arduino` - Arduino_GFX with ESP32-P4 MIPI-DSI support
- `lvgl` plus `lv_conf.h` - LVGL 9 for `04_LVGLV9_Arduino`

See the [main README](../README.md) and the [official product documentation]
(https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B) for hardware details.
