#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino_GFX_Library.h>
#include "displays_config.h"
#include "gt911.h"
#include "serial_log.h"

static esp_lcd_touch_handle_t tp_handle = NULL;
#define MAX_TOUCH_POINTS 5

uint16_t touch_x[MAX_TOUCH_POINTS] = { 0 };
uint16_t touch_y[MAX_TOUCH_POINTS] = { 0 };
uint16_t touch_strength[MAX_TOUCH_POINTS] = { 0 };
uint8_t touch_cnt = 0;

Arduino_ESP32DSIPanel *dsipanel = new Arduino_ESP32DSIPanel(
  display_cfg.hsync_pulse_width,
  display_cfg.hsync_back_porch,
  display_cfg.hsync_front_porch,
  display_cfg.vsync_pulse_width,
  display_cfg.vsync_back_porch,
  display_cfg.vsync_front_porch,
  display_cfg.prefer_speed,
  display_cfg.lane_bit_rate);
Arduino_DSI_Display *gfx = new Arduino_DSI_Display(
  display_cfg.width,
  display_cfg.height,
  dsipanel,
  0,
  true,
  display_cfg.lcd_rst,
  display_cfg.init_cmds,
  display_cfg.init_cmds_size);

void setup(void) {

  serial_log::begin(115200);
  display_cfg_backlight(true);  // turn the panel backlight on first
  serial_log::println("Arduino_GFX Hello World example");

  delay(1000);

  DEV_I2C_Port port = DEV_I2C_Init();

  tp_handle = touch_gt911_init(port);

  if (!gfx->begin()) {
    serial_log::println("gfx->begin() failed!");
  }


  gfx->fillScreen(WHITE);
}

void loop() {

  esp_lcd_touch_read_data(tp_handle);

  bool pressed = esp_lcd_touch_get_coordinates(
    tp_handle, touch_x, touch_y, touch_strength, &touch_cnt, MAX_TOUCH_POINTS);

  if (pressed && touch_cnt > 0) {
    for (int i = 0; i < touch_cnt; i++) {
      if (touch_x[i] == 1 && touch_y[i] == 1) {
      } else {
        gfx->fillCircle(touch_x[i], touch_y[i], 5, BLUE);
      }
    }
  } else {
    delay(10);
  }
}
