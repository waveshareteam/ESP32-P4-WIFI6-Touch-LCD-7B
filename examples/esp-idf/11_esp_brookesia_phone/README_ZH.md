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
cd examples/esp-idf/11_esp_brookesia_phone
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为你的开发板串口。

## 板级适配

- 将 Registry 托管的 `waveshare/esp32_p4_wifi6_touch_lcd_7b` BSP 精确固定为 `3.0.0`；不使用本地 BSP 副本或已提交的依赖覆盖。
- 直接使用 BSP 公开的高层显示配置与加锁 API，其底层由 BSP 管理的 `espressif/esp_lvgl_adapter` `~0.6` 版本线提供支持。
- ESP-Brookesia `0.4.2` 保持 LVGL 8 合同（`>=8.3,<9`）；本项目使用的 BSP 版本必须同时保留 LVGL 8 与 LVGL 9 的公开 API 变体。
- 使用 1024 x 600 Brookesia 样式表。
- 显示和摄像头尺寸使用 `BSP_LCD_H_RES` 和 `BSP_LCD_V_RES`。
- About 页面显示板名 `ESP32-P4-WIFI6-Touch-LCD-7B`。

## CI 兼容性边界

此示例暂时不纳入 ESP-IDF v5.5.5 和 v6.0.2 编译矩阵，因为 ESP-Brookesia 0.4.x 需要 LVGL 8，
而 7B BSP 使用 LVGL 9.5。上游 `esp-audio-player` 1.1.0 固定版本和显式拆分驱动依赖会在
兼容边界恢复后保持本项目源码可编译。`firmware/brookesia` 源码单独维护，目前没有 GitHub
Actions 工作流构建或打包它。
遗留 `esp_video`/`esp_ipa` 编译兼容垫片仅保持源码兼容，不验证视频、ISP 或其他硬件运行行为。
官方托管 `espressif/esp_lcd_ek79007` `2.0.2~1` 依赖提供托管 Waveshare BSP 3.0.0
显示驱动合同所需的版本守卫 ESP-IDF 5/6 API。
官方托管 `espressif/human_face_detect` `0.5.0`、`espressif/pedestrian_detect` `0.3.2` 和
`espressif/esp-dl` `3.3.9` 依赖提供兼容 ESP-IDF 5/6 的检测器加密/API 支持。GitHub Actions
仅验证编译；LCD、人脸、行人及其他硬件运行时行为未经验证。

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

`spiffs/` 目录包含演示应用使用的示例音频资源。基于本示例发布产品固件前，请替换为
具备授权的客户内容。

`sdkconfig`、`build/`、`managed_components/` 和 `dependencies.lock` 等生成文件
会被忽略，不应提交。

> [!NOTE]
> 本示例暂不进入默认 CI 矩阵：esp-brookesia `0.4.x` 依赖 LVGL 8，而 7B BSP 已迁移到 LVGL 9.5 线。
> 待 esp-brookesia 发布 LVGL 9 版本后恢复 CI 构建。
