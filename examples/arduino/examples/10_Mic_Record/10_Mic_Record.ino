/*
 * ES7210 microphone capture demo for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * Captures the dual microphones through the ES7210 ADC (I2C 0x40) and prints
 * the recorded PCM data to the serial monitor: per 30 ms frame the peak, RMS
 * and 16 decimated 16-bit samples are printed as comma-separated values.
 *
 * I2C: SDA=GPIO7, SCL=GPIO8. I2S: MCLK=GPIO13, BCLK=GPIO12, LRCK=GPIO10,
 * DIN=GPIO11.
 */
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>
#include "es7210.h"
#include "audio_hal.h"

#define PIN_I2S_MCLK 13
#define PIN_I2S_BCLK 12
#define PIN_I2S_LRCK 10
#define PIN_I2S_DIN  11

#define SAMPLE_RATE      16000
#define FRAME_LENGTH_MS  30
#define FRAME_SAMPLES    (FRAME_LENGTH_MS * SAMPLE_RATE / 1000)
#define I2S_CH           I2S_NUM_0

static int16_t *frame = NULL;

esp_err_t es7210_codec_init(void) {
  audio_hal_codec_config_t cfg = {
    .adc_input = AUDIO_HAL_ADC_INPUT_ALL,
    .codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE,
    .i2s_iface = {
      .mode = AUDIO_HAL_MODE_SLAVE,
      .fmt = AUDIO_HAL_I2S_NORMAL,
      .samples = AUDIO_HAL_16K_SAMPLES,
      .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
    },
  };
  esp_err_t ret = ESP_OK;
  ret |= es7210_adc_init(&Wire, &cfg);
  ret |= es7210_adc_config_i2s(cfg.codec_mode, &cfg.i2s_iface);
  ret |= es7210_adc_set_gain(
      (es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2),
      (es7210_gain_value_t)GAIN_0DB);
  ret |= es7210_adc_set_gain(
      (es7210_input_mics_t)(ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4),
      (es7210_gain_value_t)GAIN_37_5DB);
  ret |= es7210_adc_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);
  return ret;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(7, 8, 100000);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_16BIT,
  };
  i2s_pin_config_t pin_config = {
    .mck_io_num = PIN_I2S_MCLK,
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_LRCK,
    .data_out_num = -1,
    .data_in_num = PIN_I2S_DIN,
  };
  esp_err_t ret = i2s_driver_install(I2S_CH, &i2s_config, 0, NULL);
  if (ret != ESP_OK) {
    Serial.printf("I2S driver install failed: %s\n", esp_err_to_name(ret));
    return;
  }
  ret = i2s_set_pin(I2S_CH, &pin_config);
  if (ret != ESP_OK) {
    Serial.printf("I2S pin config failed: %s\n", esp_err_to_name(ret));
    i2s_driver_uninstall(I2S_CH);
    return;
  }
  i2s_zero_dma_buffer(I2S_CH);

  if (es7210_codec_init() != ESP_OK) {
    Serial.println("ES7210 init failed!");
    i2s_driver_uninstall(I2S_CH);
    return;
  }
  frame = (int16_t *)malloc(FRAME_SAMPLES * sizeof(int16_t));
  if (frame == NULL) {
    Serial.println("frame buffer allocation failed!");
    i2s_driver_uninstall(I2S_CH);
    return;
  }
  Serial.println("ES7210 ready; printing mic PCM frames");
}

void loop() {
  if (frame == NULL) {
    delay(1000);
    return;
  }
  size_t bytes_read = 0;
  esp_err_t ret = i2s_read(I2S_CH, (char *)frame, FRAME_SAMPLES * sizeof(int16_t), &bytes_read, portMAX_DELAY);
  if (ret != ESP_OK || bytes_read != FRAME_SAMPLES * sizeof(int16_t)) {
    Serial.printf("I2S read failed: %s, bytes: %u\n", esp_err_to_name(ret), (unsigned)bytes_read);
    delay(5);
    return;
  }

  int32_t peak = 0;
  int64_t sum_sq = 0;
  for (int i = 0; i < FRAME_SAMPLES; i++) {
    int32_t v = frame[i];
    if (v < 0) v = -v;
    if (v > peak) peak = v;
    sum_sq += (int64_t)frame[i] * frame[i];
  }
  int32_t rms = (int32_t)sqrtf((float)sum_sq / FRAME_SAMPLES);

  Serial.print(millis());
  Serial.print(" peak=");
  Serial.print(peak);
  Serial.print(" rms=");
  Serial.print(rms);
  Serial.print(" samples=");
  // decimated dump: 16 samples per frame
  for (int i = 0; i < FRAME_SAMPLES; i += FRAME_SAMPLES / 16) {
    Serial.print(frame[i]);
    Serial.print(",");
  }
  Serial.println();
}
