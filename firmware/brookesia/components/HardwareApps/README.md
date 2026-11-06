# 7B Hardware Applications

[简体中文](README_ZH.md)

`HardwareApps` contains the board-specific launcher applications that expose
the 7B on-board industrial interfaces. The peripheral access layer is in the
local `board` component; it keeps GPIO and shared-power ownership out of the
UI classes.

| Application | Interface | Launcher behavior |
| --- | --- | --- |
| RS485 Terminal | UART1, GPIO27 TX / GPIO26 RX, 115200 8N1 | Uses the on-board automatic-direction transceiver. Automatic greeting transmission is enabled by default; no RTS/DE control is configured. |
| CAN / TWAI Terminal | GPIO22 TX / GPIO21 RX, 500 kbit/s normal mode | Uses the on-board TJA1051. Automatic test transmission is disabled by default; SEND queues one frame, received frames and status are shown, and BUS_OFF triggers a recovery request. |

RS485 and CAN use fixed `ESP_3V3` GPIO domains and do not depend on VO4. VO4
supplies the complete GPIO39-GPIO48 VDD_IO_5 bank and is owned by `board`; the
board uses GPIO39-GPIO44 for SDMMC and GPIO45 for card-power control.

The repository's board-profile and validation boundaries are documented in
[`../../../../docs/ESP32P4_REVISION_CONFIG.md`](../../../../docs/ESP32P4_REVISION_CONFIG.md).
This public repository does not include a local schematic or source-file hash index.

These applications compile with the delivery firmware but were not flashed or
tested with a physical RS485/CAN peer. A real CAN bus needs peer acknowledgement
and appropriate termination; a successful UI launch or compile is not a bus
health result.
