/*
 * OV5647 MIPI-CSI camera preview for the Waveshare
 * ESP32-P4-WIFI6-Touch-LCD-7B (1024x600 DSI display).
 *
 * Uses the ESP_Video library bundled with arduino-esp32 core 3.3.11.
 * The camera SCCB bus shares the board I2C pins (SDA=7, SCL=8); the
 * MIPI-CSI lanes are wired internally. The default OV5647 sensor mode
 * streams RAW8 frames which the ISP pipeline converts to RGB565.
 *
 * The captured frame is centered on the display; when the sensor frame
 * is wider than the panel (e.g. 800 px) the horizontal margins are cropped.
 */
#ifndef BOARD_HAS_PSRAM
#error "This program requires PSRAM enabled (enable PSRAM in the Tools menu)"
#endif

#include <Arduino_GFX_Library.h>
#include <ESP_Video.h>
#include "displays_config.h"
#include "serial_log.h"

#define CAMERA_SCCB_PORT 0
#define CAMERA_SCCB_SCL  8
#define CAMERA_SCCB_SDA  7

ESPVideoClass video;
ESPVideoCaptureDevClass capture_dev;
const size_t kCaptureBufferCount = 2;

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

bool initCamera() {
  ESPVideoCamConfigClass cam_config;
  if (!cam_config.begin(CAMERA_SCCB_PORT, CAMERA_SCCB_SCL, CAMERA_SCCB_SDA)) {
    serial_log::println("SCCB config failed");
    return false;
  }

  ESPVideoCSIConfigClass csi_config;
  csi_config.begin(cam_config);
  if (!video.begin(csi_config)) {
    serial_log::println("CSI camera init failed");
    return false;
  }
  if (!capture_dev.begin(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, kCaptureBufferCount)) {
    serial_log::println("capture device open failed");
    return false;
  }
  if (!capture_dev.setFormat(ESP_VIDEO_FORMAT_RGB565)) {
    serial_log::println("RGB565 format request failed");
    return false;
  }
  if (!capture_dev.startCapture()) {
    serial_log::println("start capture failed");
    return false;
  }
  return true;
}

void setup() {
  serial_log::begin(115200);
  display_cfg_backlight(true);  // turn the panel backlight on first
  delay(200);

  if (!gfx->begin()) {
    serial_log::println("display begin failed!");
    return;
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->println("Camera preview starting...");

  if (!initCamera()) {
    gfx->setTextColor(RGB565_RED);
    gfx->println("Camera init failed - connect an OV5647 module");
  }
}

void loop() {
  if (!capture_dev.isOpened() || !capture_dev.isCaptureStarted()) {
    delay(500);
    return;
  }

  ESPVideoBufferClass buffer = capture_dev.captureBuffer();
  if (!buffer.valid()) {
    delay(5);
    return;
  }

  uint32_t w = buffer.getWidth();
  uint32_t h = buffer.getHeight();
  if (w > 0 && h > 0 && buffer.formatType() == ESP_VIDEO_FORMAT_RGB565) {
    // The camera buffer rows use the FULL capture pitch (w * 2 bytes), so a
    // cropped blit must be drawn row by row instead of as one contiguous
    // bitmap, otherwise every row drifts and the image tears diagonally.
    int16_t src_x = (int32_t)w > gfx->width() ? (w - gfx->width()) / 2 : 0;
    int16_t dst_x = (int32_t)w < gfx->width() ? (gfx->width() - w) / 2 : 0;
    int16_t dst_y = (int32_t)h < gfx->height() ? (gfx->height() - h) / 2 : 0;
    int16_t draw_w = w <= gfx->width() ? w : gfx->width();
    int16_t draw_h = h <= gfx->height() ? h : gfx->height();
    const uint16_t *pixels = (const uint16_t *)buffer.data();
    for (int16_t y = 0; y < draw_h; y++) {
      gfx->draw16bitRGBBitmap(dst_x, dst_y + y, (uint16_t *)(pixels + (size_t)y * w + src_x), draw_w, 1);
    }
  }
  // buffer is returned to the driver when it goes out of scope here
}
