# RS485 UART 回显示例

[English](README.md)

本示例通过 ESP32-P4-WIFI6-Touch-LCD-7B 板载 RS485 接口回显收到的数据。
示例目标为 `esp32p4`，并使用板载自动收发方向控制电路。

## 硬件映射

默认值依据 [官方产品原理图](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf)，
该原理图由 [Waveshare 资源页面](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B/Resources-And-Documents)提供。

| UART 功能 | Kconfig 选项 | ESP32-P4 GPIO | 原理图网络名 |
| --- | --- | --- | --- |
| 发送（TX） | `EXAMPLE_UART_TXD` | GPIO27 | `485_RXD` / 收发器 DI |
| 接收（RX） | `EXAMPLE_UART_RXD` | GPIO26 | `485_TXD` / 收发器 RO |

原理图网络名从收发器一侧命名，因此看起来与 ESP32-P4 的 UART 方向相反。
收发器的 `/RE` 和 `DE` 由板载自动方向控制电路驱动，无需单独的方向 GPIO、RTS
或 CTS 信号。

将 RS485 对端连接到开发板的 RS485 `A`、`B` 和地端子。

## 配置

运行 `idf.py menuconfig`，然后进入 **Echo Example Configuration** 修改：

| 选项 | 默认值 | 用途 |
| --- | --- | --- |
| `EXAMPLE_UART_PORT_NUM` | UART1 | UART 控制器 |
| `EXAMPLE_UART_BAUD_RATE` | 115200 | 通信速率 |
| `EXAMPLE_UART_TXD` | GPIO27 | 发送引脚 |
| `EXAMPLE_UART_RXD` | GPIO26 | 接收引脚 |
| `EXAMPLE_TASK_STACK_SIZE` | 2048 字节 | 回显任务栈 |

除非同步修改硬件连接，否则更改 TX 或 RX 引脚会使示例与板载 RS485 收发器断开。

## 构建和烧录

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

从已连接的 RS485 对端发送数据，开发板会在同一总线上返回收到的字节。编译通过
不能替代 RS485 硬件测试。

## 故障排查

- 回显出现在 RS485 接口，而不是 USB 串口监视器。
- 确认两端波特率一致，并连接共同地线。
- 无法接收数据时，检查 `A`、`B` 接线。
