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

  // Init Display
  serial_log::println("starting gfx->begin()...");
  if (!gfx->begin()) {
    serial_log::println("gfx->begin() failed!");
  } else {
    serial_log::println("gfx->begin() ok");
  }

  // Diagnostic color bars: top red / middle green / bottom blue. If you see
  // these bands the DSI panel and framebuffer are working.
  gfx->fillScreen(RGB565_RED);
  delay(1500);
  gfx->fillScreen(RGB565_GREEN);
  delay(1500);
  gfx->fillScreen(RGB565_BLUE);
  delay(1500);

  gfx->fillScreen(RGB565_BLACK);
  gfx->setCursor(10, 10);
  gfx->setTextColor(RGB565_WHITE);
  gfx->println("Hello World!");
  serial_log::println("HelloWorld on screen");

  delay(5000);  // 5 seconds
}

void loop() {
  gfx->setCursor(random(gfx->width()), random(gfx->height()));
  gfx->setTextColor(random(0xffff), random(0xffff));
  gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
  gfx->println("Hello World!");

  delay(1000);  // 1 second
}
