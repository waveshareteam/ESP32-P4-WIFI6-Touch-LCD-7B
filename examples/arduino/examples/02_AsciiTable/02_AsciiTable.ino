#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino_GFX_Library.h>
#include "displays_config.h"
#include "serial_log.h"

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

  int numCols = (display_cfg.width / 12) - 1;
  int numRows = display_cfg.height / 16;

  // Init Display
  if (!gfx->begin()) {
    serial_log::println("gfx->begin() failed!");
  }
  gfx->fillScreen(BLACK);

  gfx->setTextColor(GREEN);
  for (int x = 0; x < numCols; x++) {
    gfx->setCursor(16 + x * 12, 4);
    gfx->print(x);
  }
  gfx->setTextColor(BLUE);
  for (int y = 0; y < numRows; y++) {
    gfx->setCursor(4, 16 + y * 16);
    gfx->print(y);
  }

  char c = 0;
  for (int y = 0; y < numRows; y++) {
    for (int x = 0; x < numCols; x++) {
      gfx->drawChar(16 + x * 12, 16 + y * 16, c++, WHITE, BLACK);
    }
  }

  delay(5000);  // 5 seconds
}

void loop() {
}
