/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * The following example demonstrates a master node in a TWAI network. The master
 * node is responsible for initiating and stopping the transfer of data messages.
 * The example will execute multiple iterations, with each iteration the master
 * node will do the following:
 * 1) Start the TWAI driver
 * 2) Repeatedly send ping messages until a ping response from slave is received
 * 3) Send start command to slave and receive data messages from slave
 * 4) Send stop command to slave and wait for stop response from slave
 * 5) Stop the TWAI driver
 */
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h"
#include <esp_timer.h>  
  
#include "driver/i2c.h"
#include "driver/gpio.h"

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<CONFIG_EXAMPLE_TX_GPIO_NUM) | (1ULL<<CONFIG_EXAMPLE_RX_GPIO_NUM))

// #define I2C_MASTER_SCL_IO           9      /*!< GPIO number used for I2C master clock */
// #define I2C_MASTER_SDA_IO           8      /*!< GPIO number used for I2C master data  */
// #define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
// #define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
// #define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
// #define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
// #define I2C_MASTER_TIMEOUT_MS       1000

/* --------------------- Definitions and static variables ------------------ */
//Example Configuration
#define TX_GPIO_NUM             CONFIG_EXAMPLE_TX_GPIO_NUM
#define RX_GPIO_NUM             CONFIG_EXAMPLE_RX_GPIO_NUM
#define EXAMPLE_TAG             "TWAI Master"

static bool driver_installed = false;
unsigned long previousMillis = 0;  // will store last time a message was send
// Intervall:
#define TRANSMIT_RATE_MS 1000

#define POLLING_RATE_MS 1000

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NO_ACK);

// static esp_err_t i2c_master_init(void)
// {
//     int i2c_master_port = I2C_MASTER_NUM;

//     i2c_config_t conf = {
//         .mode = I2C_MODE_MASTER,
//         .sda_io_num = I2C_MASTER_SDA_IO,
//         .scl_io_num = I2C_MASTER_SCL_IO,
//         .sda_pullup_en = GPIO_PULLUP_ENABLE,
//         .scl_pullup_en = GPIO_PULLUP_ENABLE,
//         .master.clk_speed = I2C_MASTER_FREQ_HZ,
//     };

//     i2c_param_config(i2c_master_port, &conf);

//     return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
// }

static void send_message() {
  // Send message

  // Configure message to transmit
  twai_message_t message;
  message.identifier = 0x0F6;
  message.data_length_code = 8;
  for (int i = 0; i < 8; i++) {
    message.data[i] = i;
  }

  // Queue message for transmission
  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    printf("Message queued for transmission\n");
  } else {
    printf("Failed to queue message for transmission\n");
  }
}

void app_main(void)
{
    /*
	//zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO20/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    // gpio_config(&io_conf);
	*/
    
	// ESP_ERROR_CHECK(i2c_master_init());
    // int ret;
    // uint8_t write_buf = 0x01;

    // ret = i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    // write_buf = 0x20;
    // ret = i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    // Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        ESP_LOGI(EXAMPLE_TAG,"Driver installed");
    } else {
        ESP_LOGI(EXAMPLE_TAG,"Failed to install driver");
        return;
    }
    
    // Start TWAI driver
    if (twai_start() == ESP_OK) {
        ESP_LOGI(EXAMPLE_TAG,"Driver started");
    } else {
        ESP_LOGI(EXAMPLE_TAG,"Failed to start driver");
        return;
    }

    // Reconfigure alerts to detect TX alerts and Bus-Off errors
    uint32_t alerts_to_enable = TWAI_ALERT_TX_IDLE | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR;
    if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
        ESP_LOGI(EXAMPLE_TAG,"CAN Alerts reconfigured");
    } else {
        ESP_LOGI(EXAMPLE_TAG,"Failed to reconfigure alerts");
        return;
    }

    // TWAI driver is now successfully installed and started
    driver_installed = true;
    while (1)
    {
        if (!driver_installed) {
            // Driver not installed
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }
        // Check if alert happened
        uint32_t alerts_triggered;
        twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(POLLING_RATE_MS));
        twai_status_info_t twaistatus;
        twai_get_status_info(&twaistatus);

        // Handle alerts
        if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
            ESP_LOGI(EXAMPLE_TAG,"Alert: TWAI controller has become error passive.");
        }
        if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
            ESP_LOGI(EXAMPLE_TAG,"Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
            ESP_LOGI(EXAMPLE_TAG,"Bus error count: %"PRIu32, twaistatus.bus_error_count);
        }
        if (alerts_triggered & TWAI_ALERT_TX_FAILED) {
            ESP_LOGI(EXAMPLE_TAG,"Alert: The Transmission failed.");
            ESP_LOGI(EXAMPLE_TAG,"TX buffered: %"PRIu32, twaistatus.msgs_to_tx);
            ESP_LOGI(EXAMPLE_TAG,"TX error: %"PRIu32, twaistatus.tx_error_counter);
            ESP_LOGI(EXAMPLE_TAG,"TX failed: %"PRIu32, twaistatus.tx_failed_count);
        }
        if (alerts_triggered & TWAI_ALERT_TX_SUCCESS) {
            ESP_LOGI(EXAMPLE_TAG,"Alert: The Transmission was successful.");
            ESP_LOGI(EXAMPLE_TAG,"TX buffered: %"PRIu32, twaistatus.msgs_to_tx);
        }

        // Send message
        unsigned long currentMillis = esp_timer_get_time() / 1000;
        if (currentMillis - previousMillis >= TRANSMIT_RATE_MS) {
            previousMillis = currentMillis;
            send_message();
        }
    }
    
}
