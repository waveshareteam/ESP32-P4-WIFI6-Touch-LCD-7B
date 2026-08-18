/*
 * microSD card read/write demo for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * The microSD slot is wired to the SDIO interface:
 *   CLK=GPIO43, CMD=GPIO44, D0=GPIO39, D1=GPIO40, D2=GPIO41, D3=GPIO42
 */
#include <Arduino.h>
#include <SD_MMC.h>
#include <FS.h>

#define SD_CLK 43
#define SD_CMD 44
#define SD_D0  39
#define SD_D1  40
#define SD_D2  41
#define SD_D3  42

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)) {
    Serial.println("SD_MMC pin configuration failed");
    return;
  }
  if (!SD_MMC.begin("/sdcard", false /* 4-bit */)) {
    Serial.println("Card mount failed - insert a FAT32-formatted microSD card");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  Serial.printf("Card type: %s\n",
    cardType == CARD_MMC ? "MMC" : cardType == CARD_SD ? "SDSC" : cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
  Serial.printf("Card size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
  Serial.printf("Total space: %llu MB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %llu MB\n", SD_MMC.usedBytes() / (1024 * 1024));

  const char *path = "/hello_lcd7b.txt";
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open file for writing");
    return;
  }
  f.println("Hello from ESP32-P4-WIFI6-Touch-LCD-7B microSD demo!");
  f.close();
  Serial.println("Wrote /hello_lcd7b.txt");

  f = SD_MMC.open(path, FILE_READ);
  if (f) {
    Serial.println("Read back:");
    while (f.available()) {
      Serial.write(f.read());
    }
    f.close();
  }

  Serial.println("Listing root directory:");
  File root = SD_MMC.open("/");
  File entry = root.openNextFile();
  while (entry) {
    Serial.printf("  %s (%u bytes)\n", entry.name(), (unsigned)entry.size());
    entry = root.openNextFile();
  }
  root.close();
  Serial.println("SD card demo finished");
}

void loop() {
  delay(1000);
}
