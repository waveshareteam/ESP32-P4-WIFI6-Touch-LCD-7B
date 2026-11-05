#include "SettingsStorage.hpp"

#include <algorithm>
#include "sdkconfig.h"
#include "esp_brookesia.hpp"

#define ESP_UTILS_LOG_TAG "BS:SettingsStorage"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps::settings_storage {

namespace {

// NVS keys are limited to 15 characters.
constexpr char KEY_BRIGHTNESS[] = "brightness";
constexpr char KEY_VOLUME[] = "volume";

int load(const char *key, int default_value, int min_value, int max_value)
{
    int value = default_value;
#if CONFIG_ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS
    services::StorageNVS::Value stored;
    if (services::StorageNVS::requestInstance().getLocalParam(key, stored) && std::holds_alternative<int>(stored)) {
        value = std::get<int>(stored);
    }
#else
    (void)key;
#endif
    return std::clamp(value, min_value, max_value);
}

void save(const char *key, int value)
{
#if CONFIG_ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS
    // Asynchronous: the service writes NVS from its own thread.
    if (!services::StorageNVS::requestInstance().setLocalParam(key, value)) {
        ESP_UTILS_LOGW("Save %s failed", key);
    }
#else
    (void)key;
    (void)value;
#endif
}

} // namespace

int loadBrightness()
{
    return load(KEY_BRIGHTNESS, BRIGHTNESS_DEFAULT, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
}

void saveBrightness(int percent)
{
    save(KEY_BRIGHTNESS, std::clamp(percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX));
}

int loadVolume()
{
    return load(KEY_VOLUME, VOLUME_DEFAULT, VOLUME_MIN, VOLUME_MAX);
}

void saveVolume(int percent)
{
    save(KEY_VOLUME, std::clamp(percent, VOLUME_MIN, VOLUME_MAX));
}

} // namespace esp_brookesia::apps::settings_storage
