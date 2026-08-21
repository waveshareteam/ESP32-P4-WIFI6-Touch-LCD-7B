# RS485 UART Echo

[简体中文](README_ZH.md)

This example echoes data through the onboard RS485 interface of the
ESP32-P4-WIFI6-Touch-LCD-7B. It targets `esp32p4` and uses the board's
automatic transmit/receive direction circuit.

## Hardware Mapping

The defaults follow the [official product schematic](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf),
linked from the [Waveshare resources page](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B/Resources-And-Documents).

| UART function | Kconfig option | ESP32-P4 GPIO | Schematic net |
| --- | --- | --- | --- |
| Transmit (TX) | `EXAMPLE_UART_TXD` | GPIO27 | `485_RXD` / transceiver DI |
| Receive (RX) | `EXAMPLE_UART_RXD` | GPIO26 | `485_TXD` / transceiver RO |

The schematic net names describe the transceiver side and therefore appear
opposite to the ESP32-P4 UART direction. The transceiver's `/RE` and `DE`
inputs are controlled by the onboard automatic-direction circuit; no separate
direction GPIO, RTS signal, or CTS signal is required.

Connect an RS485 peer to the board's RS485 `A`, `B`, and ground terminals.

## Configuration

Run `idf.py menuconfig`, then open **Echo Example Configuration** to change:

| Option | Default | Purpose |
| --- | --- | --- |
| `EXAMPLE_UART_PORT_NUM` | UART1 | UART controller |
| `EXAMPLE_UART_BAUD_RATE` | 115200 | Link speed |
| `EXAMPLE_UART_TXD` | GPIO27 | Transmit pin |
| `EXAMPLE_UART_RXD` | GPIO26 | Receive pin |
| `EXAMPLE_TASK_STACK_SIZE` | 2048 bytes | Echo task stack |

Changing the TX or RX pins disconnects the example from the onboard RS485
transceiver unless the hardware is modified accordingly.

## Build and Flash

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Type data from the connected RS485 peer. The board returns the received bytes
on the same bus. Build success does not replace an RS485 hardware test.

## Troubleshooting

- The echo appears on the RS485 connector, not on the USB serial monitor.
- Verify that both devices use the same baud rate and share a ground reference.
- Check the `A` and `B` connections if no data is received.
