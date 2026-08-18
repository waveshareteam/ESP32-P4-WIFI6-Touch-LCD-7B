# I2S Codec

[简体中文](README_ZH.md)

Audio codec example for the board speaker and microphone path.

The example uses the managed board BSP and `esp_codec_dev` for the board's
ES8311 speaker and ES7210 microphone paths. The default mode plays sample PCM;
menuconfig can select a microphone-to-speaker echo mode.

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
| `CONFIG_EXAMPLE_MODE_MUSIC` | Play the embedded PCM sample |
| `CONFIG_EXAMPLE_MODE_ECHO` | Echo microphone input to the speaker |
| `CONFIG_EXAMPLE_MIC_GAIN_DB` | Echo-mode microphone gain |

CI compiles this example with ESP-IDF v5.5.5 and v6.0.2 against managed BSP
version 3.0.0. Compilation does not verify audio runtime behavior on hardware.
