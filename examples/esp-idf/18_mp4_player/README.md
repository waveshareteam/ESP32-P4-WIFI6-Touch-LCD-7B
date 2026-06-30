# MP4 Player

[中文](README_CN.md)

This example plays an MP4 or AVI file from the microSD card on the
ESP32-P4-WIFI6-Touch-LCD-7B 1024 x 600 MIPI-DSI LCD.

The playback path is adapted from
[ESP32-P4-WIFI6-Touch-LCD-5 `10_mp4_player`](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-5/tree/main/examples/esp-idf/10_mp4_player).
It uses the 7B BSP, the ESP32-P4 hardware JPEG decoder, the board SD card
interface, and the onboard audio codec when an audio track is present.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- microSD card formatted with a FAT filesystem.
- USB cable for power, flashing, and serial logs.

## Video Requirements

- File location: `/sdcard/test_video.mp4` by default.
- Container: MP4 or AVI.
- Video codec: MJPEG. H.264 and H.265 are not supported by this example.
- Recommended resolution: `1024x600` for the 7B LCD.
- Recommended frame rate: 15 to 20 fps.
- Audio: AAC or MP3 in MP4 files is supported when the board audio codec is available.

Convert a source video with FFmpeg:

```bash
ffmpeg -i input.mp4 -c:v mjpeg -q:v 5 -vf scale=1024:600 -r 20 -c:a aac test_video.mp4
```

Copy `test_video.mp4` to the root directory of the microSD card.

## Build and Flash

This demo is verified with ESP-IDF v5.5.4. The CI matrix keeps it on v5.5.4
because the bundled extractor library is provided for that release series.

```bash
cd examples/esp-idf/18_mp4_player
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Configuration

Run `idf.py menuconfig` and open `MP4 Player Configuration` to change:

- `Video File Name`: default `test_video.mp4`.
- Optional audio decoder switches.
- Audio-video synchronization.

The example loops the selected file continuously. If the file is missing or uses
an unsupported codec, the serial log prints the reason and playback stops.

Generated files such as `sdkconfig`, `build/`, `managed_components/`, and
`dependencies.lock` are ignored and should not be committed.
