/*
 * ES8311 melody player for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * Plays the opening of "Für Elise" (Beethoven) through the ES8311 codec and
 * the on-board 2 W speaker, synthesizing each note as a sine wave with a
 * simple decay envelope.
 *
 * I2C: SDA=GPIO7, SCL=GPIO8 (codec 0x18). I2S: MCLK=GPIO13, BCLK=GPIO12,
 * LRCK=GPIO10, DOUT=GPIO9. Speaker amp: GPIO53 (high = on).
 */
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "esp_check.h"
#include "es8311.h"

#define PIN_POWER_AMP 53
#define PIN_I2S_MCLK  13
#define PIN_I2S_BCLK  12
#define PIN_I2S_LRCK  10
#define PIN_I2S_DOUT  9

#define SAMPLE_RATE      16000
#define TONE_FRAME       64
#define NOTE_AMPLITUDE   8000
#define MELODY_VOLUME    90
#define I2S_CH           I2S_NUM_0

struct Note {
  float freq;
  uint16_t duration_ms;
};

// Für Elise, opening section (A). 0 = rest.
static const Note FUR_ELISE[] = {
  {659.25f, 350}, {622.25f, 350}, {659.25f, 350}, {622.25f, 350}, {659.25f, 350},
  {493.88f, 350}, {587.33f, 350}, {523.25f, 350}, {440.00f, 700},
  {261.63f, 350}, {329.63f, 350}, {440.00f, 350}, {493.88f, 700},
  {329.63f, 350}, {415.30f, 350}, {493.88f, 350}, {523.25f, 700},
  {329.63f, 350}, {659.25f, 350}, {622.25f, 350}, {659.25f, 350}, {622.25f, 350},
  {659.25f, 350}, {493.88f, 350}, {587.33f, 350}, {523.25f, 350}, {440.00f, 900},
};

esp_err_t es8311_codec_init(void) {
  es8311_handle_t es_handle = es8311_create(0, ES8311_ADDRRES_0);
  ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, "ES8311", "create failed");
  const es8311_clock_config_t es_clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = SAMPLE_RATE * 256,
    .sample_frequency = SAMPLE_RATE
  };
  ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
  ESP_ERROR_CHECK(es8311_sample_frequency_config(es_handle, es_clk.mclk_frequency, es_clk.sample_frequency));
  ESP_ERROR_CHECK(es8311_microphone_config(es_handle, false));
  ESP_ERROR_CHECK(es8311_voice_volume_set(es_handle, MELODY_VOLUME, NULL));
  return ESP_OK;
}

void play_note(float freq, uint32_t duration_ms) {
  uint32_t samples = (uint32_t)((uint64_t)SAMPLE_RATE * duration_ms / 1000);
  uint32_t samples_played = 0;
  float phase = 0;
  const float phase_step = freq * 2.0f * (float)PI / SAMPLE_RATE;
  int16_t frame[TONE_FRAME * 2];
  while (samples_played < samples) {
    uint32_t chunk = samples - samples_played < TONE_FRAME ? samples - samples_played : TONE_FRAME;
    for (uint32_t i = 0; i < chunk; i++) {
      // simple exponential decay envelope over the note
      float progress = (float)(samples_played + i) / (float)samples;
      float envelope = (progress < 0.15f) ? progress / 0.15f : 1.0f - 0.6f * (progress - 0.15f) / 0.85f;
      int16_t sample = (int16_t)(sinf(phase) * NOTE_AMPLITUDE * envelope);
      frame[i * 2] = sample;
      frame[i * 2 + 1] = sample;
      phase += phase_step;
      if (phase > 2.0f * (float)PI) phase -= 2.0f * (float)PI;
    }
    size_t written = 0;
    i2s_write(I2S_CH, frame, chunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    samples_played += chunk;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_POWER_AMP, OUTPUT);
  digitalWrite(PIN_POWER_AMP, HIGH);
  Wire.begin(7, 8, 100000);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
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
    .data_out_num = PIN_I2S_DOUT,
    .data_in_num = -1,
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

  if (es8311_codec_init() != ESP_OK) {
    Serial.println("ES8311 init failed!");
    i2s_driver_uninstall(I2S_CH);
    return;
  }
  Serial.println("ES8311 ready, playing Fuer Elise");
}

void loop() {
  for (const Note &note : FUR_ELISE) {
    play_note(note.freq, note.duration_ms);
    delay(60);  // small gap between notes
  }
  delay(1200);  // pause before repeating
}
