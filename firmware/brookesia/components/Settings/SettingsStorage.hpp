#pragma once

#include "board_audio.h"

/*
 * Persisted user settings, stored through the Brookesia StorageNVS service.
 * The service must have been started (StorageNVS::begin()) before load*() is
 * called; until then, and when the service is disabled, the defaults are used.
 */
namespace esp_brookesia::apps::settings_storage {

constexpr int BRIGHTNESS_MIN = 20;
constexpr int BRIGHTNESS_MAX = 100;
constexpr int BRIGHTNESS_DEFAULT = 100;

constexpr int VOLUME_MIN = 0;
constexpr int VOLUME_MAX = 95;
constexpr int VOLUME_DEFAULT = CODEC_DEFAULT_VOLUME;

/** Saved brightness in percent, clamped to [BRIGHTNESS_MIN, BRIGHTNESS_MAX]. */
int loadBrightness();
void saveBrightness(int percent);

/** Saved media volume in percent, clamped to [VOLUME_MIN, VOLUME_MAX]. */
int loadVolume();
void saveVolume(int percent);

} // namespace esp_brookesia::apps::settings_storage
