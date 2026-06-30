# I2S Codec

Audio codec example for the board speaker and microphone path.

The example uses the board BSP/audio codec configuration and plays sample PCM
data through the I2S codec path.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.
- Speaker or audio output path connected to the board codec.

## Build and Flash

```bash
cd examples/esp-idf/06_i2s_codec
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## Configuration

Useful menuconfig options:

| Option | Purpose |
| --- | --- |
| `CONFIG_EXAMPLE_VOICE_VOLUME` | Playback volume |
| `CONFIG_EXAMPLE_BSP` | Enable BSP-based hardware setup |

The example is currently checked in CI with ESP-IDF v5.5.4 while upstream audio
dependencies are being kept aligned for ESP-IDF v6.
