# ESP32-P4 芯片版本配置

[English](ESP32P4_REVISION_CONFIG.md)

仓库在 [`config/`](../config/) 中只保留两个机器可读的 ESP32-P4 硅片 profile。全部
19 个 ESP-IDF 示例默认使用 `rev1_3`。ESP-IDF 的最小芯片版本选项以 1.0 为粒度，
因此 rev1.3 系列使用 `REV_MIN_100`，并不表示最小值精确为 1.3。

| Profile | defaults | 已解析符号 | Arduino 设置 |
| --- | --- | --- | --- |
| `rev1_3` | `esp32p4_rev1_3.defaults` | pre-v3 + `REV_MIN_100` | `ChipVariant=prev3` |
| `rev3_x` | `esp32p4_rev3_x.defaults` | post-v3 + `REV_MIN_300` | `ChipVariant=postv3` |

[`examples/arduino/`](../examples/arduino/) 下的 Arduino 示例必须选择与硅片相符的
`Chip Variant` 菜单选项。Arduino CI 工作流编译 `prev3` profile；使用 rev3.x 硅片时，
请在 Arduino IDE 中选择 `postv3`。两个 profile 的固件不能混用。ESP-IDF CI 包会检查
已解析的 `sdkconfig`，而不只检查请求的文件名。硅片版本不能证明 PCB/电气版本或
硬件运行；仓库没有本地原理图，板级 glue 仍以在线 BSP/应用边界为准。

两个 Brookesia profile 共用同一套自定义分区：分区表位于 `0x9000`，NVS 从 `0xa000`
开始，PHY 初始化从 `0x10000` 开始，factory app 自动对齐到 `0x20000`。这样可为 rev3
bootloader 留出空间；其大小可能超过分区表位于 `0x8000` 时可用的 `0x6000` 空间。
