# ESP32-P4 芯片版本配置

[English](ESP32P4_REVISION_CONFIG.md)

仓库在 [`config/`](../config/) 中只保留两个机器可读的 ESP32-P4 硅片 profile。纳入
矩阵的 18 个 ESP-IDF 示例默认使用 `rev3_x`，其中包括 LVGL 9 的 Brookesia phone
示例。ESP-IDF 的最小芯片版本选项以 1.0 为粒度，因此 rev1.3 系列使用
`REV_MIN_100`，并不表示最小值精确为 1.3。

| Profile | defaults | 已解析符号 | Arduino 设置 |
| --- | --- | --- | --- |
| `rev1_3` | `esp32p4_rev1_3.defaults` | pre-v3 + `REV_MIN_100` | `ChipVariant=prev3` |
| `rev3_x` | `esp32p4_rev3_x.defaults` | post-v3 + `REV_MIN_300` | `ChipVariant=postv3` |

[`examples/arduino/`](../examples/arduino/) 下的 Arduino 示例必须选择与硅片相符的
`Chip Variant` 菜单选项。Arduino CI 默认编译 `postv3`；只有 rev1.3 硅片才选择
`prev3`。Arduino-ESP32 3.3.11 菜单把 `postv3` 标为“v3.00 or newer”，但 production
P4 预编译库实际写入 `REV_MIN_FULL=301`。因此编译通过不能证明恰好 v3.0 的样片能够
启动该库，烧录前仍需确认实际硅片。

两个 profile 的固件不能混用。ESP-IDF CI 检查已解析的 `sdkconfig`，而不只检查
defaults 文件名。手动构建 rev1.3 时，可把 `config/esp32p4_rev1_3.defaults` 追加在
项目 defaults 之后；后加载的 profile 会解析为 pre-v3 与 `REV_MIN_100`。

两个 profile 的屏幕几何与链路时序均为 1024 x 600、52 MHz DPI、双 DSI lane 且每
lane 1000 Mbit/s。随硅片版本变化的是 PHY 时钟源：ESP-IDF 为 pre-v3 构建选择
legacy PLL_F20M，为 Rev3.x 构建选择默认 XTAL。Arduino DSI 驱动将 `phy_clk_src`
保持为官方的 revision-aware 零值，由 `Chip Variant` 选中的预编译库在编译时做出
相同选择；这不是运行时硅片探测。

使用 performance 优化的示例 defaults 会把 bootloader 日志降为 `WARN`，使 Rev3.x
QIO bootloader 保持在默认 `0x8000` 分区表之前的 `0x6000` 空间内；这只改变
bootloader 日志详细程度，不改变应用日志级别或分区布局。

硅片版本不能证明 PCB/电气版本或硬件运行。本次提交不包含本地原理图，板级 glue
继续使用已发布的 BSP/应用边界。编译结果不等同于屏幕、触摸、RS485 或 CAN HIL。

两个 Brookesia profile 共用同一套自定义分区：分区表位于 `0x9000`，NVS 从
`0xa000` 开始，PHY 初始化从 `0x10000` 开始，factory app 自动对齐到 `0x20000`。
这样可为 rev3 bootloader 留出空间；其大小可能超过分区表位于 `0x8000` 时可用的
`0x6000` 空间。
