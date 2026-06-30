# MP4 播放器

[English](README.md)

本示例从 microSD 卡读取 MP4 或 AVI 文件，并在
ESP32-P4-WIFI6-Touch-LCD-7B 的 1024 x 600 MIPI-DSI 屏上循环播放。

播放链路参考
[ESP32-P4-WIFI6-Touch-LCD-5 的 `10_mp4_player`](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-5/tree/main/examples/esp-idf/10_mp4_player)
适配而来。当前版本使用 7B BSP、ESP32-P4 硬件 JPEG 解码器、板载 SD 卡接口，
并在音频轨存在且音频 codec 初始化成功时播放声音。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- FAT 文件系统格式的 microSD 卡。
- 用于供电、烧录和查看串口日志的 USB 线。

## 视频要求

- 默认文件位置：`/sdcard/test_video.mp4`。
- 容器格式：MP4 或 AVI。
- 视频编码：MJPEG。本示例不支持 H.264、H.265 等视频编码。
- 推荐分辨率：`1024x600`，匹配 7B 屏幕。
- 推荐帧率：15 到 20 fps。
- 音频：MP4 内的 AAC 或 MP3 音轨可在板载音频 codec 可用时播放。

可使用 FFmpeg 转码：

```bash
ffmpeg -i input.mp4 -c:v mjpeg -q:v 5 -vf scale=1024:600 -r 20 -c:a aac test_video.mp4
```

将 `test_video.mp4` 复制到 microSD 卡根目录。

## 构建和烧录

本示例已使用 ESP-IDF v5.5.4 验证。由于示例内置的 extractor 库面向该
版本系列提供，CI 矩阵会将本示例固定在 v5.5.4。

```bash
cd examples/esp-idf/18_mp4_player
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

请将 `PORT` 替换为开发板对应的串口。

## 配置

运行 `idf.py menuconfig`，打开 `MP4 Player Configuration` 可修改：

- `Video File Name`：默认 `test_video.mp4`。
- 可选音频解码器开关。
- 音视频同步开关。

示例会持续循环播放选中的文件。如果文件不存在或编码不受支持，串口日志会输出原因并停止播放。

`sdkconfig`、`build/`、`managed_components/` 和 `dependencies.lock` 等生成文件
会被忽略，不应提交。
