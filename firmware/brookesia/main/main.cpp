#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "product_audio.h"

#include "esp_brookesia.hpp"
#include "app_examples/phone/squareline/src/phone_app_squareline.hpp"
#include "apps.h"

static const char *TAG = "main";

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(bsp_spiffs_mount());
    ESP_LOGI(TAG, "SPIFFS mount successfully");

#if CONFIG_EXAMPLE_ENABLE_SD_CARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    ESP_LOGI(TAG, "SD card mount successfully");
#endif

    ESP_ERROR_CHECK(product_audio_codec_init());

    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_180,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == nullptr) {
        ESP_LOGE(TAG, "Failed to start BSP display");
        return;
    }

    ESP_ERROR_CHECK(bsp_display_lock(-1) ? ESP_OK : ESP_ERR_TIMEOUT);

    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone();
    assert(phone != nullptr && "Failed to create phone");

    ESP_Brookesia_PhoneStylesheet_t *phone_stylesheet = new ESP_Brookesia_PhoneStylesheet_t ESP_BROOKESIA_PHONE_1024_600_DARK_STYLESHEET();
    if (phone_stylesheet == nullptr) {
        ESP_LOGE(TAG, "Failed to create phone stylesheet");
        bsp_display_unlock();
        return;
    }
    if (!phone->addStylesheet(*phone_stylesheet)) {
        ESP_LOGE(TAG, "Failed to add phone stylesheet");
        bsp_display_unlock();
        return;
    }
    if (!phone->activateStylesheet(*phone_stylesheet)) {
        ESP_LOGE(TAG, "Failed to activate phone stylesheet");
        bsp_display_unlock();
        return;
    }

    if (!phone->begin()) {
        ESP_LOGE(TAG, "Failed to begin phone");
        bsp_display_unlock();
        return;
    }

    PhoneAppSquareline *smart_gadget = new PhoneAppSquareline();
    assert(smart_gadget != nullptr && "Failed to create phone app squareline");
    if (phone->installApp(smart_gadget) < 0) {
        ESP_LOGE(TAG, "Failed to install phone app squareline");
        bsp_display_unlock();
        return;
    }

    Calculator *calculator = new Calculator();
    assert(calculator != nullptr && "Failed to create calculator");
    if (phone->installApp(calculator) < 0) {
        ESP_LOGE(TAG, "Failed to install calculator");
        bsp_display_unlock();
        return;
    }

    MusicPlayer *music_player = new MusicPlayer();
    assert(music_player != nullptr && "Failed to create music_player");
    if (phone->installApp(music_player) < 0) {
        ESP_LOGE(TAG, "Failed to install music_player");
        bsp_display_unlock();
        return;
    }

    AppSettings *app_settings = new AppSettings();
    assert(app_settings != nullptr && "Failed to create app_settings");
    if (phone->installApp(app_settings) < 0) {
        ESP_LOGE(TAG, "Failed to install app_settings");
        bsp_display_unlock();
        return;
    }

    Game2048 *game_2048 = new Game2048();
    assert(game_2048 != nullptr && "Failed to create game_2048");
    if (phone->installApp(game_2048) < 0) {
        ESP_LOGE(TAG, "Failed to install game_2048");
        bsp_display_unlock();
        return;
    }

    Camera *camera = new Camera(BSP_LCD_H_RES, BSP_LCD_V_RES);
    assert(camera != nullptr && "Failed to create camera");
    if (phone->installApp(camera) < 0) {
        ESP_LOGE(TAG, "Failed to install camera");
        bsp_display_unlock();
        return;
    }

#if CONFIG_EXAMPLE_ENABLE_SD_CARD
    ESP_LOGW(TAG, "Using Video Player example requires inserting the SD card in advance and saving an MJPEG format video on the SD card");
    AppVideoPlayer *app_video_player = new AppVideoPlayer();
    assert(app_video_player != nullptr && "Failed to create app_video_player");
    if (phone->installApp(app_video_player) < 0) {
        ESP_LOGE(TAG, "Failed to install app_video_player");
        bsp_display_unlock();
        return;
    }
#endif

    bsp_display_unlock();
    bsp_display_backlight_on();

}
