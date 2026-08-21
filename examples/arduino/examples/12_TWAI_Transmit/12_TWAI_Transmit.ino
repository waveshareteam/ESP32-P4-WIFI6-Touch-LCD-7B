/*
 * CAN/TWAI transmit demo for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * GPIO22 (TWAI TX) and GPIO21 (TWAI RX) are already routed to the on-board
 * TJA1051. Connect a correctly terminated peer or analyzer to the CANH/CANL
 * connector so transmitted frames receive an ACK.
 */
#include <Arduino.h>
#include <inttypes.h>
#include "driver/twai.h"

constexpr gpio_num_t TWAI_TX_PIN = GPIO_NUM_22;
constexpr gpio_num_t TWAI_RX_PIN = GPIO_NUM_21;
constexpr uint32_t TWAI_BITRATE = 500000;
constexpr uint32_t TRANSMIT_INTERVAL_MS = 1000;

static bool twai_ready = false;
static uint32_t sequence_number = 0;
static uint32_t previous_transmit_ms = 0;

static void print_status() {
  twai_status_info_t status = {};
  if (twai_get_status_info(&status) != ESP_OK) {
    return;
  }

  Serial.printf(
    "TWAI state=%d tx_queue=%" PRIu32 " tx_failed=%" PRIu32 " bus_errors=%" PRIu32 "\n",
    static_cast<int>(status.state),
    status.msgs_to_tx,
    status.tx_failed_count,
    status.bus_error_count);
}

static void report_alerts(uint32_t alerts) {
  if (alerts & TWAI_ALERT_TX_SUCCESS) {
    Serial.println("TWAI frame acknowledged");
  }
  if (alerts & TWAI_ALERT_TX_FAILED) {
    Serial.println("TWAI transmission was not acknowledged");
  }
  if (alerts & TWAI_ALERT_ERR_PASS) {
    Serial.println("TWAI controller entered error-passive state");
  }
  if (alerts & TWAI_ALERT_BUS_ERROR) {
    Serial.println("TWAI bus error detected");
  }
  if (alerts & TWAI_ALERT_BUS_OFF) {
    Serial.println("TWAI bus-off detected; initiating recovery");
    twai_initiate_recovery();
  }
  if (alerts & TWAI_ALERT_RECOVERY_IN_PROGRESS) {
    Serial.println("TWAI recovery in progress");
  }
  if (alerts & TWAI_ALERT_BUS_RECOVERED) {
    Serial.println("TWAI bus recovered");
  }
}

static bool start_twai() {
  twai_general_config_t general_config =
    TWAI_GENERAL_CONFIG_DEFAULT(TWAI_TX_PIN, TWAI_RX_PIN, TWAI_MODE_NORMAL);
  general_config.tx_queue_len = 5;
  general_config.rx_queue_len = 5;
  twai_timing_config_t timing_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t filter_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t result = twai_driver_install(&general_config, &timing_config, &filter_config);
  if (result != ESP_OK) {
    Serial.printf("TWAI driver install failed: %s\n", esp_err_to_name(result));
    return false;
  }

  result = twai_start();
  if (result != ESP_OK) {
    Serial.printf("TWAI driver start failed: %s\n", esp_err_to_name(result));
    twai_driver_uninstall();
    return false;
  }

  const uint32_t alerts =
    TWAI_ALERT_TX_SUCCESS |
    TWAI_ALERT_TX_FAILED |
    TWAI_ALERT_ERR_PASS |
    TWAI_ALERT_BUS_ERROR |
    TWAI_ALERT_BUS_OFF |
    TWAI_ALERT_RECOVERY_IN_PROGRESS |
    TWAI_ALERT_BUS_RECOVERED;
  result = twai_reconfigure_alerts(alerts, nullptr);
  if (result != ESP_OK) {
    Serial.printf("TWAI alert configuration failed: %s\n", esp_err_to_name(result));
    twai_stop();
    twai_driver_uninstall();
    return false;
  }

  return true;
}

static void send_frame() {
  twai_message_t frame = {};
  frame.identifier = 0x07B;
  frame.data_length_code = 8;
  frame.data[0] = 0x7B;
  frame.data[1] = 0x50;
  frame.data[2] = static_cast<uint8_t>(sequence_number >> 24);
  frame.data[3] = static_cast<uint8_t>(sequence_number >> 16);
  frame.data[4] = static_cast<uint8_t>(sequence_number >> 8);
  frame.data[5] = static_cast<uint8_t>(sequence_number);
  frame.data[6] = static_cast<uint8_t>(millis() >> 8);
  frame.data[7] = static_cast<uint8_t>(millis());

  const esp_err_t result = twai_transmit(&frame, pdMS_TO_TICKS(100));
  if (result == ESP_OK) {
    Serial.printf("Queued CAN frame id=0x%03X sequence=%" PRIu32 "\n", frame.identifier, sequence_number++);
  } else {
    Serial.printf("Failed to queue CAN frame: %s\n", esp_err_to_name(result));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.printf(
    "TWAI transmit: TX=GPIO%d RX=GPIO%d bitrate=%lu\n",
    static_cast<int>(TWAI_TX_PIN),
    static_cast<int>(TWAI_RX_PIN),
    static_cast<unsigned long>(TWAI_BITRATE));
  twai_ready = start_twai();
  if (twai_ready) {
    Serial.println("TWAI ready; connect a terminated CAN bus node or analyzer.");
  }
}

void loop() {
  if (!twai_ready) {
    delay(1000);
    return;
  }

  uint32_t alerts = 0;
  if (twai_read_alerts(&alerts, pdMS_TO_TICKS(20)) == ESP_OK && alerts != 0) {
    report_alerts(alerts);
    print_status();
  }

  const uint32_t now = millis();
  if (now - previous_transmit_ms >= TRANSMIT_INTERVAL_MS) {
    previous_transmit_ms = now;
    send_frame();
  }
}
