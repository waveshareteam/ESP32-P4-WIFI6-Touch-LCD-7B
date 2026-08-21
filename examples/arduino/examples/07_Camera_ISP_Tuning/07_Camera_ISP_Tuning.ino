/*
 * OV5647 MIPI-CSI ISP / 3A tuning demo for the Waveshare
 * ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * Live preview plus interactive sensor/ISP controls through the serial
 * monitor. The ISP pipeline of the ESP_Video library exposes the common
 * 3A knobs through V4L2 extended controls:
 *
 *   g <0..1023>   sensor analog gain
 *   e <0..10000>  sensor exposure time (us)
 *   a <0..255>    AE target level
 *   v <0|1>       vertical flip
 *   h <0|1>       horizontal flip
 *   t <0|1>       sensor test pattern
 *   s             print current settings
 *
 * Example: "g 128" doubles the brightness, "a 64" brightens the AE target.
 * Values are forwarded to the driver only when they change; the HUD on the
 * display shows the last accepted settings.
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

struct IspSettings {
  int32_t gain = 0;         // 0 = driver default
  int32_t exposure = 0;     // 0 = driver default
  int32_t ae_target = 0;    // 0 = driver default
  int32_t vflip = -1;
  int32_t hflip = -1;
  int32_t test_pattern = -1;
} settings;

static void apply(IspSettings &s) {
  if (s.gain > 0) capture_dev.setSensorGain(s.gain);
  if (s.exposure > 0) capture_dev.setSensorExposureTime(s.exposure);
  if (s.ae_target > 0) capture_dev.setSensorAETargetLevel(s.ae_target);
  if (s.vflip >= 0) capture_dev.setSensorVFlip(s.vflip != 0);
  if (s.hflip >= 0) capture_dev.setSensorHFlip(s.hflip != 0);
  if (s.test_pattern >= 0) capture_dev.setSensorTestPattern(s.test_pattern != 0);
}

static void showSettings() {
  serial_log::printf(
    "gain=%ld exposure_us=%ld ae_target=%ld vflip=%ld hflip=%ld test_pattern=%ld\n",
    (long)settings.gain, (long)settings.exposure, (long)settings.ae_target,
    (long)settings.vflip, (long)settings.hflip, (long)settings.test_pattern);
}

static void handleSerial() {
  static String cmd;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      cmd.trim();
      if (cmd.length() > 0) {
        char op = cmd[0];
        long value = 0;
        if (cmd.length() > 2) value = cmd.substring(2).toInt();
        switch (op) {
          case 'g': settings.gain = value; apply(settings); break;
          case 'e': settings.exposure = value; apply(settings); break;
          case 'a': settings.ae_target = value; apply(settings); break;
          case 'v': settings.vflip = value; apply(settings); break;
          case 'h': settings.hflip = value; apply(settings); break;
          case 't': settings.test_pattern = value; apply(settings); break;
          case 's': break;
          default: serial_log::println("unknown command");
        }
        showSettings();
      }
      cmd = "";
    } else {
      cmd += c;
    }
  }
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

  ESPVideoCamConfigClass cam_config;
  cam_config.begin(CAMERA_SCCB_PORT, CAMERA_SCCB_SCL, CAMERA_SCCB_SDA);
  ESPVideoCSIConfigClass csi_config;
  csi_config.begin(cam_config);
  if (!video.begin(csi_config) ||
      !capture_dev.begin(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, kCaptureBufferCount) ||
      !capture_dev.setFormat(ESP_VIDEO_FORMAT_RGB565) ||
      !capture_dev.startCapture()) {
    serial_log::println("camera pipeline init failed");
    return;
  }
  serial_log::println("ISP tuning ready. Commands: g/e/a/v/h/t/s (see sketch header)");
  showSettings();
}

void loop() {
  handleSerial();

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
    int16_t src_x = (int32_t)w > gfx->width() ? (w - gfx->width()) / 2 : 0;
    int16_t dst_x = (int32_t)w < gfx->width() ? (gfx->width() - w) / 2 : 0;
    int16_t dst_y = (int32_t)h < gfx->height() ? (gfx->height() - h) / 2 : 0;
    int16_t draw_w = w <= gfx->width() ? w : gfx->width();
    int16_t draw_h = h <= gfx->height() ? h : gfx->height();
    const uint16_t *pixels = (const uint16_t *)buffer.data();
    for (int16_t y = 0; y < draw_h; y++) {
      gfx->draw16bitRGBBitmap(dst_x, dst_y + y, (uint16_t *)(pixels + (size_t)y * w + src_x), draw_w, 1);
    }

    // HUD overlay
    gfx->setTextColor(RGB565_BLACK, RGB565_WHITE);
    gfx->setCursor(4, 4);
    gfx->printf("gain=%ld exp=%ldus ae=%ld", (long)settings.gain, (long)settings.exposure, (long)settings.ae_target);
  }
}
