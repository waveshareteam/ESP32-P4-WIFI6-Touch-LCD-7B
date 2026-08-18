/*
 * RS485 echo for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * The onboard transceiver is wired to UART1:
 *   ESP32-P4 TX GPIO27 -> transceiver DI / board net 485_RXD
 *   ESP32-P4 RX GPIO26 <- transceiver RO / board net 485_TXD
 *
 * Its automatic direction circuit controls /RE and DE. Do not enable UART
 * RS485 half-duplex mode or attach RTS/DE in this sketch.
 */
#include <Arduino.h>
#include "esp_ldo_regulator.h"

constexpr int RS485_TX_PIN = 27;
constexpr int RS485_RX_PIN = 26;
constexpr uint32_t RS485_BAUD = 115200;
constexpr size_t RX_BUFFER_SIZE = 256;

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static esp_ldo_channel_handle_t rs485_ldo = nullptr;

static bool enable_rs485_transceiver() {
  if (rs485_ldo != nullptr) {
    return true;
  }

  esp_ldo_channel_config_t ldo_config = {};
  ldo_config.chan_id = 4;
  ldo_config.voltage_mv = 3300;
  const esp_err_t result = esp_ldo_acquire_channel(&ldo_config, &rs485_ldo);
  if (result != ESP_OK) {
    Serial.printf("RS485 LDO VO4 enable failed: %s\n", esp_err_to_name(result));
    return false;
  }

  Serial.println("RS485 LDO VO4 set to 3300 mV");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!enable_rs485_transceiver()) {
    return;
  }

  Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  if (!Serial1) {
    Serial.println("RS485 UART1 initialization failed");
    return;
  }

  Serial.printf(
    "RS485 echo ready: UART1 TX=GPIO%d RX=GPIO%d baud=%lu\n",
    RS485_TX_PIN,
    RS485_RX_PIN,
    static_cast<unsigned long>(RS485_BAUD));
  Serial.println("Connect an RS485 peer to A, B, and GND, then send data.");
}

void loop() {
  size_t received = 0;
  while (Serial1.available() > 0 && received < sizeof(rx_buffer)) {
    const int value = Serial1.read();
    if (value >= 0) {
      rx_buffer[received++] = static_cast<uint8_t>(value);
    }
  }

  if (received == 0) {
    delay(2);
    return;
  }

  const size_t written = Serial1.write(rx_buffer, received);
  Serial1.flush();
  Serial.printf("Echoed %u/%u RS485 byte(s)\n", static_cast<unsigned>(written), static_cast<unsigned>(received));
}
