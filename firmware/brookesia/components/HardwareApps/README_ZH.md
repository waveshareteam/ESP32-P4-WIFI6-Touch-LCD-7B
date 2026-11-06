# 7B 硬件应用

[English](README.md)

`HardwareApps` 包含面向 7B 板载工业接口的桌面应用。外设访问层位于本地
`board` 组件，使 GPIO 和共享电源的归属不落入 UI 类。

| 应用 | 接口 | 桌面行为 |
| --- | --- | --- |
| RS485 Terminal | UART1，GPIO27 TX / GPIO26 RX，115200 8N1 | 使用板载自动方向收发器；默认开启自动发送问候语，不配置 RTS/DE 控制。 |
| CAN / TWAI Terminal | GPIO22 TX / GPIO21 RX，500 kbit/s normal mode | 使用板载 TJA1051；默认关闭自动发送测试帧，SEND 发送一个帧，显示接收帧和状态，并在 BUS_OFF 后请求恢复。 |

RS485 与 CAN 使用固定的 `ESP_3V3` GPIO 电源域，不依赖 VO4。VO4 为整个
GPIO39-GPIO48 VDD_IO_5 域供电并由 `board` 持有；开发板使用 GPIO39-GPIO44 连接
SDMMC，GPIO45 控制卡电源。

仓库中的板级 profile 和验证边界见
[`../../../../docs/ESP32P4_REVISION_CONFIG_ZH.md`](../../../../docs/ESP32P4_REVISION_CONFIG_ZH.md)。
此公开仓库不包含本地原理图或源文件哈希索引。

这些应用随交付固件一起编译，但未烧录，也没有和实体 RS485/CAN 对端进行测试。真实
CAN 总线需要对端确认和适当终端；成功打开 UI 或编译不能说明总线健康。
