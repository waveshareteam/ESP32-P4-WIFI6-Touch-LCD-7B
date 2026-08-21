# I2S Codec

[English](README.md)

面向开发板扬声器和麦克风链路的音频 codec 示例。

示例通过管理型开发板 BSP 和 `esp_codec_dev` 使用板载 ES8311 扬声器与 ES7210
麦克风链路。默认模式播放示例 PCM；可在 menuconfig 中选择麦克风到扬声器的回声模式。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。
- 连接到板载 codec 的扬声器或音频输出链路。

## 构建和烧录

```bash
cd examples/esp-idf/06_i2s_codec
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

## 配置

常用 menuconfig 选项：

| 选项 | 用途 |
| --- | --- |
| `CONFIG_EXAMPLE_VOICE_VOLUME` | 播放音量 |
| `CONFIG_EXAMPLE_MODE_MUSIC` | 播放内嵌 PCM 示例 |
| `CONFIG_EXAMPLE_MODE_ECHO` | 将麦克风输入回放到扬声器 |
| `CONFIG_EXAMPLE_MIC_GAIN_DB` | 回声模式的麦克风增益 |

CI 使用 ESP-IDF v5.5.5 和 v6.0.2，并基于管理型 BSP 3.0.1 编译本示例。
编译不代表已在硬件上验证音频运行行为。
