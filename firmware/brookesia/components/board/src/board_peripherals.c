/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "board_peripherals.h"

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOARD_RS485_UART UART_NUM_1
#define BOARD_RS485_TX_GPIO (GPIO_NUM_27)
#define BOARD_RS485_RX_GPIO (GPIO_NUM_26)
#define BOARD_TWAI_TX_GPIO (GPIO_NUM_22)
#define BOARD_TWAI_RX_GPIO (GPIO_NUM_21)
#define BOARD_RS485_RX_BUFFER_SIZE (2048)
#define BOARD_RS485_TX_BUFFER_SIZE (2048)

static const char *TAG = "board_periph";
static bool s_rs485_initialized;
static bool s_twai_initialized;

esp_err_t board_rs485_init(void)
{
    if (s_rs485_initialized) {
        return ESP_OK;
    }

    const uart_config_t config = {
        .baud_rate = BOARD_RS485_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(BOARD_RS485_UART, BOARD_RS485_RX_BUFFER_SIZE, BOARD_RS485_TX_BUFFER_SIZE, 0, NULL, 0), TAG, "Install RS485 UART driver failed");

    esp_err_t ret = uart_param_config(BOARD_RS485_UART, &config);
    if (ret == ESP_OK) {
        ret = uart_set_pin(BOARD_RS485_UART, BOARD_RS485_TX_GPIO, BOARD_RS485_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (ret == ESP_OK) {
        ret = uart_flush_input(BOARD_RS485_UART);
    }
    if (ret != ESP_OK) {
        uart_driver_delete(BOARD_RS485_UART);
        return ret;
    }

    /* The 7B THVD1406/MAX13487 circuit controls DE and /RE in hardware. */
    s_rs485_initialized = true;
    return ESP_OK;
}

esp_err_t board_rs485_deinit(void)
{
    if (!s_rs485_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = uart_wait_tx_done(BOARD_RS485_UART, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
        return ret;
    }
    ret = uart_driver_delete(BOARD_RS485_UART);
    if (ret == ESP_OK) {
        s_rs485_initialized = false;
    }
    return ret;
}

esp_err_t board_rs485_write(const void *data, size_t size, size_t *bytes_written)
{
    ESP_RETURN_ON_FALSE(data && size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid RS485 TX buffer");
    ESP_RETURN_ON_ERROR(board_rs485_init(), TAG, "RS485 initialization failed");

    const int written = uart_write_bytes(BOARD_RS485_UART, data, size);
    ESP_RETURN_ON_FALSE(written >= 0, ESP_FAIL, TAG, "RS485 write failed");
    if (bytes_written) {
        *bytes_written = (size_t)written;
    }
    return written == (int)size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t board_rs485_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(data && size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid RS485 RX buffer");
    ESP_RETURN_ON_ERROR(board_rs485_init(), TAG, "RS485 initialization failed");

    const int read = uart_read_bytes(BOARD_RS485_UART, data, size, pdMS_TO_TICKS(timeout_ms));
    ESP_RETURN_ON_FALSE(read >= 0, ESP_FAIL, TAG, "RS485 read failed");
    if (bytes_read) {
        *bytes_read = (size_t)read;
    }
    return ESP_OK;
}

esp_err_t board_twai_init(void)
{
    if (s_twai_initialized) {
        return ESP_OK;
    }

    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(BOARD_TWAI_TX_GPIO, BOARD_TWAI_RX_GPIO, TWAI_MODE_NORMAL);
    general.tx_queue_len = 8;
    general.rx_queue_len = 16;
    general.alerts_enabled = TWAI_ALERT_RX_DATA | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_ERR_PASS;
    const twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_RETURN_ON_ERROR(twai_driver_install(&general, &timing, &filter), TAG, "Install TWAI driver failed");
    esp_err_t ret = twai_start();
    if (ret != ESP_OK) {
        twai_driver_uninstall();
        return ret;
    }
    s_twai_initialized = true;
    return ESP_OK;
}

esp_err_t board_twai_deinit(void)
{
    if (!s_twai_initialized) {
        return ESP_OK;
    }

    twai_status_info_t status = {};
    ESP_RETURN_ON_ERROR(twai_get_status_info(&status), TAG, "Read TWAI status failed");
    if (status.state == TWAI_STATE_RUNNING) {
        ESP_RETURN_ON_ERROR(twai_stop(), TAG, "Stop TWAI driver failed");
    }
    if (status.state == TWAI_STATE_RECOVERING) {
        for (int i = 0; i < 50; ++i) {
            vTaskDelay(pdMS_TO_TICKS(20));
            ESP_RETURN_ON_ERROR(twai_get_status_info(&status), TAG, "Read TWAI recovery status failed");
            if (status.state != TWAI_STATE_RECOVERING) {
                break;
            }
        }
        if (status.state == TWAI_STATE_RECOVERING) {
            /* No legacy-driver API safely cancels recovery. Keep this board-wide
             * driver alive; a later run will resume once the bus becomes idle. */
            ESP_LOGW(TAG, "TWAI recovery still active; preserving driver for later resume");
            return ESP_OK;
        }
    }
    esp_err_t ret = twai_driver_uninstall();
    if (ret == ESP_OK) {
        s_twai_initialized = false;
    }
    return ret;
}

esp_err_t board_twai_transmit(const twai_message_t *message, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(message, ESP_ERR_INVALID_ARG, TAG, "CAN message is NULL");
    ESP_RETURN_ON_ERROR(board_twai_init(), TAG, "TWAI initialization failed");
    return twai_transmit(message, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t board_twai_receive(twai_message_t *message, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(message, ESP_ERR_INVALID_ARG, TAG, "CAN message is NULL");
    ESP_RETURN_ON_ERROR(board_twai_init(), TAG, "TWAI initialization failed");
    return twai_receive(message, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t board_twai_get_status(board_twai_status_t *status)
{
    ESP_RETURN_ON_FALSE(status, ESP_ERR_INVALID_ARG, TAG, "TWAI status is NULL");
    ESP_RETURN_ON_ERROR(board_twai_init(), TAG, "TWAI initialization failed");

    twai_status_info_t info = {};
    ESP_RETURN_ON_ERROR(twai_get_status_info(&info), TAG, "Read TWAI status failed");
    *status = (board_twai_status_t){
        .state = info.state,
        .tx_error_counter = info.tx_error_counter,
        .rx_error_counter = info.rx_error_counter,
        .tx_failed_count = info.tx_failed_count,
        .rx_missed_count = info.rx_missed_count,
        .bus_error_count = info.bus_error_count,
    };
    return ESP_OK;
}

esp_err_t board_twai_recover(void)
{
    ESP_RETURN_ON_ERROR(board_twai_init(), TAG, "TWAI initialization failed");

    twai_status_info_t status = {};
    ESP_RETURN_ON_ERROR(twai_get_status_info(&status), TAG, "Read TWAI status failed");
    if (status.state == TWAI_STATE_BUS_OFF) {
        return twai_initiate_recovery();
    }
    if (status.state == TWAI_STATE_STOPPED) {
        return twai_start();
    }
    return ESP_OK;
}
