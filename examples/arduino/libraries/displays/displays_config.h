// displays_config.h - Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
#pragma once
#ifndef DISPLAYS_CONFIG_H
#define DISPLAYS_CONFIG_H

#include <Arduino_GFX_Library.h>
#include "i2c.h"

struct DisplayConfig {
  const char *name;

  uint32_t hsync_pulse_width;
  uint32_t hsync_back_porch;
  uint32_t hsync_front_porch;
  uint32_t vsync_pulse_width;
  uint32_t vsync_back_porch;
  uint32_t vsync_front_porch;
  uint32_t prefer_speed;
  uint32_t lane_bit_rate;

  uint16_t width;
  uint16_t height;
  int8_t rotation;
  bool auto_flush;
  int8_t rst_pin;

  const lcd_init_cmd_t *init_cmds;
  size_t init_cmds_size;

  int8_t i2c_sda_pin;
  int8_t i2c_scl_pin;
  uint32_t i2c_clock_speed;
  int8_t lcd_rst;
};

// The 7B backlight is active low. Match the BSP's 5 kHz, 10-bit inverted
// LEDC output so a duty of 1023 produces full brightness.
#define LCD7B_BACKLIGHT_PIN ((int8_t)32)
#define LCD7B_BACKLIGHT_FREQ 5000
#define LCD7B_BACKLIGHT_RES 10

inline bool display_cfg_backlight(bool on) {
  static bool configured = false;
  if (!configured) {
    if (!ledcAttach(LCD7B_BACKLIGHT_PIN, LCD7B_BACKLIGHT_FREQ, LCD7B_BACKLIGHT_RES) ||
        !ledcOutputInvert(LCD7B_BACKLIGHT_PIN, true)) {
      return false;
    }
    configured = true;
  }

  return ledcWrite(
    LCD7B_BACKLIGHT_PIN,
    on ? ((1U << LCD7B_BACKLIGHT_RES) - 1U) : 0U);
}

// EK79007 1024x600 2-lane MIPI-DSI initialization. This follows the board
// BSP: set the 2-lane pad control, apply the vendor registers, then sleep out.
static const lcd_init_cmd_t vendor_specific_init_default[] = {
  {0xB2, (uint8_t[]){0x10}, 1, 0},
  {0x80, (uint8_t[]){0x8B}, 1, 0},
  {0x81, (uint8_t[]){0x78}, 1, 0},
  {0x82, (uint8_t[]){0x84}, 1, 0},
  {0x83, (uint8_t[]){0x88}, 1, 0},
  {0x84, (uint8_t[]){0xA8}, 1, 0},
  {0x85, (uint8_t[]){0xE3}, 1, 0},
  {0x86, (uint8_t[]){0x88}, 1, 0},
  {0x11, nullptr, 0, 120},
};

// 7-inch LCD profile from waveshare/esp32_p4_wifi6_touch_lcd_7b:
// 1024x600 at 60 Hz, 52 MHz DPI clock and two DSI lanes at 1 Gbps.
const DisplayConfig SCREEN_DEFAULT = {
  .name = "LCD7B-DSI",
  .hsync_pulse_width = 10,
  .hsync_back_porch = 160,
  .hsync_front_porch = 160,
  .vsync_pulse_width = 1,
  .vsync_back_porch = 23,
  .vsync_front_porch = 12,
  .prefer_speed = 52000000,
  .lane_bit_rate = 1000,
  .width = 1024,
  .height = 600,
  .rotation = 0,
  .auto_flush = true,
  .rst_pin = 33,
  .init_cmds = vendor_specific_init_default,
  .init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(lcd_init_cmd_t),
  .i2c_sda_pin = 7,
  .i2c_scl_pin = 8,
  .i2c_clock_speed = 100000,
  .lcd_rst = 33,
};

inline const DisplayConfig &display_cfg = SCREEN_DEFAULT;
#endif
