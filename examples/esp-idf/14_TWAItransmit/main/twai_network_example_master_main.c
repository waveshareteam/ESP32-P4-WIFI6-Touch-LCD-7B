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
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

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

static bool node_enabled = false;
static twai_node_handle_t twai_node = NULL;
static volatile uint32_t tx_success_count = 0;
static volatile uint32_t tx_failed_count = 0;
static volatile uint32_t bus_error_count = 0;
unsigned long previousMillis = 0;  // will store last time a message was send
// Intervall:
#define TRANSMIT_RATE_MS 1000

#define POLLING_RATE_MS 1000

static bool twai_tx_done_callback(twai_node_handle_t handle, const twai_tx_done_event_data_t *edata, void *user_ctx)
{
    if (edata->is_tx_success) {
        tx_success_count++;
    } else {
        tx_failed_count++;
    }

    return false;
}

static bool twai_error_callback(twai_node_handle_t handle, const twai_error_event_data_t *edata, void *user_ctx)
{
    bus_error_count++;
    return false;
}

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
  uint8_t data[8];
  for (int i = 0; i < 8; i++) {
    data[i] = i;
  }
  twai_frame_t frame = {
    .header.id = 0x0F6,
    .buffer = data,
    .buffer_len = sizeof(data),
  };

  // Queue message for transmission
  if (twai_node_transmit(twai_node, &frame, 1000) == ESP_OK) {
    twai_node_transmit_wait_all_done(twai_node, 1000);
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

    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = TX_GPIO_NUM,
            .rx = RX_GPIO_NUM,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = 500000,
        },
        .fail_retry_cnt = 3,
        .tx_queue_depth = 5,
        .flags.enable_self_test = true,
    };
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &twai_node));

    twai_event_callbacks_t callbacks = {
        .on_tx_done = twai_tx_done_callback,
        .on_error = twai_error_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(twai_node, &callbacks, NULL));
    ESP_ERROR_CHECK(twai_node_enable(twai_node));
    ESP_LOGI(EXAMPLE_TAG, "Node started");

    // TWAI driver is now successfully installed and started
    node_enabled = true;
    while (1)
    {
        if (!node_enabled) {
            // Node not enabled
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }

        twai_node_status_t twai_status;
        twai_node_record_t twai_record;
        twai_node_get_info(twai_node, &twai_status, &twai_record);
        ESP_LOGI(EXAMPLE_TAG, "TX success: %" PRIu32 ", TX failed: %" PRIu32 ", bus errors: %" PRIu32,
                 tx_success_count, tx_failed_count, twai_record.bus_err_num);
        if (twai_status.state == TWAI_ERROR_BUS_OFF) {
            ESP_LOGW(EXAMPLE_TAG, "TWAI node entered bus-off state.");
            return;
        }

        // Send message
        unsigned long currentMillis = esp_timer_get_time() / 1000;
        if (currentMillis - previousMillis >= TRANSMIT_RATE_MS) {
            previousMillis = currentMillis;
            send_message();
        }
    }
    
}
