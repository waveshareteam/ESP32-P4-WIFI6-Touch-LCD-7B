/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "board_check";

static void print_chip_features(uint32_t features)
{
    printf("Features:");
    printf("%s", (features & CHIP_FEATURE_WIFI_BGN) ? " Wi-Fi" : "");
    printf("%s", (features & CHIP_FEATURE_BT) ? " BT" : "");
    printf("%s", (features & CHIP_FEATURE_BLE) ? " BLE" : "");
    printf("%s", (features & CHIP_FEATURE_IEEE802154) ? " 802.15.4" : "");
    printf("%s", (features & CHIP_FEATURE_EMB_FLASH) ? " embedded-flash" : "");
    printf("%s", (features & CHIP_FEATURE_EMB_PSRAM) ? " embedded-psram" : "");
    printf("\n");
}

void app_main(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;

    esp_chip_info(&chip_info);

    printf("\n");
    printf("========================================\n");
    printf(" ESP32-P4 Platform Board Check\n");
    printf("========================================\n");
    printf("IDF target: %s\n", CONFIG_IDF_TARGET);
    printf("CPU cores: %d\n", chip_info.cores);
    printf("Silicon revision: v%d.%d\n", chip_info.revision / 100, chip_info.revision % 100);
    print_chip_features(chip_info.features);

    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        printf("Flash size: %" PRIu32 " MB\n", flash_size / (1024U * 1024U));
    } else {
        ESP_LOGW(TAG, "Failed to read flash size");
    }

    if (esp_psram_is_initialized()) {
        printf("PSRAM: initialized\n");
        printf("PSRAM free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else {
        printf("PSRAM: not initialized or not enabled\n");
    }

    printf("Internal heap free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("Minimum free heap: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
    printf("\n");
    printf("Board check is running. Open the serial monitor and confirm this output.\n");
    printf("Next steps:\n");
    printf("  1. Run 02_HelloWorld to verify a classic ESP-IDF example.\n");
    printf("  2. Run 08_i2c_tools if you want to scan the I2C bus.\n");
    printf("  3. Choose Wi-Fi, Ethernet, SD card, display, audio, or camera examples based on your board.\n");
    printf("\n");

    while (true) {
        ESP_LOGI(TAG, "alive: free internal heap=%u bytes, free psram=%u bytes",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
