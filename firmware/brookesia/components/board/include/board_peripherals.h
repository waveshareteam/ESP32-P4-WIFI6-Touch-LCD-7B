/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/twai.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ESP32-P4-WIFI6-Touch-LCD-7B schematic-backed peripheral routing. */
#define BOARD_RS485_BAUD_RATE         (115200)

#define BOARD_TWAI_BAUD_RATE          (500000)

typedef struct {
    twai_state_t state;
    uint32_t tx_error_counter;
    uint32_t rx_error_counter;
    uint32_t tx_failed_count;
    uint32_t rx_missed_count;
    uint32_t bus_error_count;
} board_twai_status_t;

/** Initialize UART1 for the on-board automatic-direction RS485 transceiver. */
esp_err_t board_rs485_init(void);

/** Stop and uninstall UART1 after all RS485 users have stopped. */
esp_err_t board_rs485_deinit(void);

/** Write bytes to the RS485 UART. No RTS/DE pin or UART RS485 mode is used. */
esp_err_t board_rs485_write(const void *data, size_t size, size_t *bytes_written);

/** Read bytes from the RS485 UART. */
esp_err_t board_rs485_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms);

/** Initialize the on-board TJA1051-backed TWAI controller at 500 kbit/s. */
esp_err_t board_twai_init(void);

/** Stop and uninstall the TWAI driver after all TWAI users have stopped. */
esp_err_t board_twai_deinit(void);

/** Queue a standard or extended CAN frame for transmission. */
esp_err_t board_twai_transmit(const twai_message_t *message, uint32_t timeout_ms);

/** Receive one CAN frame. */
esp_err_t board_twai_receive(twai_message_t *message, uint32_t timeout_ms);

/** Copy the latest driver status. */
esp_err_t board_twai_get_status(board_twai_status_t *status);

/** Recover a BUS_OFF controller and restart it after recovery completes. */
esp_err_t board_twai_recover(void);

#ifdef __cplusplus
}
#endif
