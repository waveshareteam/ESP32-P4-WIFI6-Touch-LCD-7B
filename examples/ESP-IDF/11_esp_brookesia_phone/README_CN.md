# ESP-Brookesia Phone

[English](README.md)

本示例在 ESP32-P4-WIFI6-Touch-LCD-7B 上运行 ESP-Brookesia 手机风格 UI。它演示
7 英寸 1024 x 600 开发板上的显示、触摸、音频、摄像头、Hosted Wi-Fi、SPIFFS
媒体资源，以及可选的 SD 卡 MJPEG 视频播放。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于供电、烧录和串口监视器的 USB 线。
- 可选 SD 卡，用于 MJPEG 视频播放。

## 构建和烧录

```bash
cd examples/ESP-IDF/11_esp_brookesia_phone
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为你的开发板串口。

## 板级适配

- 使用 `waveshare/esp32_p4_wifi6_touch_lcd_7b`。
- 使用 1024 x 600 Brookesia 样式表。
- 显示和摄像头尺寸使用 `BSP_LCD_H_RES` 和 `BSP_LCD_V_RES`。
- About 页面显示板名 `ESP32-P4-WIFI6-Touch-LCD-7B`。

## 可选视频播放

如需启用 Video Player app，请插入 SD 卡并在 menuconfig 中开启 SD 卡选项：

```bash
idf.py menuconfig
```

然后选择 `Example Configuration` > `Enable SD Card`。

视频文件应使用 MJPEG 格式。常用转换命令如下：

```bash
ffmpeg -i input.mp4 -vcodec mjpeg -q:v 2 -vf "scale=1024:600" -acodec copy output.mjpeg
```

## 媒体资源

`spiffs/` 目录包含 demo app 使用的示例音频资源。基于本示例发布产品固件前，请替换为
具备授权的客户内容。

`sdkconfig`、`build/`、`managed_components/` 和 `dependencies.lock` 等生成文件
会被忽略，不应提交。
