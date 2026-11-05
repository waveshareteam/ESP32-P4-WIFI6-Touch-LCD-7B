/* SPDX-License-Identifier: Apache-2.0 */

#include "XiaozhiDeveloperTools.hpp"

#include <cstdio>
#include <cstdlib>
#include <mutex>

#include "Camera.hpp"
#include "SettingsStorage.hpp"
#include "board.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_mcp_engine.h"
#include "esp_mcp_tool.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_xiaozhi_camera.h"
#include "sdkconfig.h"

namespace esp_brookesia::apps {
namespace {
// Leave time for the tool reply and the spoken confirmation before the restart.
constexpr uint64_t REBOOT_DELAY_US = 3 * 1000 * 1000;

std::mutex camera_mutex;
esp_xiaozhi_chat_handle_t camera_chat = 0;
std::mutex reboot_mutex;
esp_timer_handle_t reboot_timer = nullptr;

esp_mcp_value_t explainImage(const esp_mcp_property_list_t *properties)
{
    std::lock_guard<std::mutex> lock(camera_mutex);
    if (!camera_chat) return esp_mcp_value_create_string("Camera unavailable: chat is stopped");
    const char *question = esp_mcp_property_list_get_property_string(properties, "question");
    if (!question || !question[0]) return esp_mcp_value_create_string("A question is required");
    char url[256] = {};
    char token[128] = {};
    esp_err_t ret = esp_xiaozhi_chat_get_vision_explain(camera_chat, url, sizeof(url), token, sizeof(token));
    if (ret != ESP_OK) return esp_mcp_value_create_string("Vision service is not available");
    uint8_t *jpeg = nullptr;
    size_t jpeg_len = 0;
    ret = Camera::requestInstance()->captureJpeg(&jpeg, &jpeg_len);
    if (ret != ESP_OK) return esp_mcp_value_create_string("Camera capture failed or camera is busy");
    esp_xiaozhi_camera_config_t config = {};
    config.explain_url = url;
    config.explain_token = token;
    esp_xiaozhi_camera_handle_t *camera = nullptr;
    ret = esp_xiaozhi_camera_create(&config, &camera);
    char *response = static_cast<char *>(heap_caps_calloc(1, 4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!response && ret == ESP_OK) ret = ESP_ERR_NO_MEM;
    if (ret == ESP_OK) {
        esp_xiaozhi_camera_frame_t frame = {};
        frame.data = jpeg;
        frame.len = jpeg_len;
        ret = esp_xiaozhi_camera_explain(camera, &frame, question, response, 4096, nullptr);
    }
    if (camera) esp_xiaozhi_camera_destroy(camera);
    free(jpeg);
    if (ret != ESP_OK) {
        free(response);
        return esp_mcp_value_create_string("Vision upload failed");
    }
    esp_mcp_value_t result = esp_mcp_value_create_string(response);
    free(response);
    return result;
}

esp_mcp_value_t jsonValue(cJSON *root)
{
    char *text = root ? cJSON_PrintUnformatted(root) : nullptr;
    cJSON_Delete(root);
    if (!text) return esp_mcp_value_create_string("{}");
    esp_mcp_value_t result = esp_mcp_value_create_string(text);
    cJSON_free(text);
    return result;
}

// Volume and brightness use the Settings ranges and are saved, so they survive a reboot like Settings changes.
esp_mcp_value_t setVolume(const esp_mcp_property_list_t *properties)
{
    const int value = esp_mcp_property_list_get_property_int(properties, "volume");
    if (value < settings_storage::VOLUME_MIN || value > settings_storage::VOLUME_MAX ||
            board_codec_volume_set(value, nullptr) != ESP_OK) {
        return esp_mcp_value_create_bool(false);
    }
    settings_storage::saveVolume(value);
    return esp_mcp_value_create_bool(true);
}

esp_mcp_value_t setBrightness(const esp_mcp_property_list_t *properties)
{
    const int value = esp_mcp_property_list_get_property_int(properties, "brightness");
    if (value < settings_storage::BRIGHTNESS_MIN || value > settings_storage::BRIGHTNESS_MAX ||
            board_display_brightness_set(value) != ESP_OK) {
        return esp_mcp_value_create_bool(false);
    }
    settings_storage::saveBrightness(value);
    return esp_mcp_value_create_bool(true);
}

esp_mcp_value_t setMute(const esp_mcp_property_list_t *properties)
{
    const bool mute = esp_mcp_property_list_get_property_bool(properties, "mute");
    return esp_mcp_value_create_bool(board_codec_mute_set(mute) == ESP_OK);
}

// Turning the screen back on restores the saved brightness instead of the board's 100 %.
esp_mcp_value_t setScreen(const esp_mcp_property_list_t *properties)
{
    const bool on = esp_mcp_property_list_get_property_bool(properties, "on");
    const esp_err_t ret = on ? board_display_brightness_set(settings_storage::loadBrightness()) :
                          board_display_backlight_off();
    return esp_mcp_value_create_bool(ret == ESP_OK);
}

esp_mcp_value_t getStatus(const esp_mcp_property_list_t *)
{
    char status[192];
    std::snprintf(status, sizeof(status),
                  "{\"volume\":%d,\"brightness\":%d,\"internal_free_bytes\":%lu,\"psram_free_bytes\":%lu}",
                  board_codec_volume_get(), board_display_brightness_get(),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    return esp_mcp_value_create_string(status);
}

esp_mcp_value_t getWifiStatus(const esp_mcp_property_list_t *)
{
    static constexpr const char *state_names[] = {"idle", "connecting", "associated", "connected"};
    board_wifi_link_t link = {};
    board_wifi_service_get_link(&link);
    const size_t state = static_cast<size_t>(link.state);

    cJSON *root = cJSON_CreateObject();
    if (!root) return esp_mcp_value_create_string("{}");
    cJSON_AddBoolToObject(root, "enabled", board_wifi_service_is_enabled());
    cJSON_AddStringToObject(root, "state", state < sizeof(state_names) / sizeof(state_names[0]) ?
                            state_names[state] : "unknown");
    cJSON_AddStringToObject(root, "ssid", link.ssid);

    esp_netif_ip_info_t ip_info = {};
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        char ip[16];
        std::snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info.ip));
        cJSON_AddStringToObject(root, "ip", ip);
    }
    return jsonValue(root);
}

esp_mcp_value_t getDeviceInfo(const esp_mcp_property_list_t *)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return esp_mcp_value_create_string("{}");

    esp_chip_info_t chip = {};
    esp_chip_info(&chip);
    char chip_text[32];
    std::snprintf(chip_text, sizeof(chip_text), "%s rev%d.%d", CONFIG_IDF_TARGET,
                  chip.revision / 100, chip.revision % 100);
    cJSON_AddStringToObject(root, "chip", chip_text);
    cJSON_AddStringToObject(root, "firmware_version", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "idf_version", esp_get_idf_version());

    board_display_info_t display = {};
    if (board_display_get_info(&display) == ESP_OK) {
        cJSON *item = cJSON_AddObjectToObject(root, "display");
        cJSON_AddNumberToObject(item, "width", display.width);
        cJSON_AddNumberToObject(item, "height", display.height);
    }

    board_mic_layout_t mics = {};
    if (board_audio_get_mic_layout(&mics) == ESP_OK) {
        cJSON *item = cJSON_AddObjectToObject(root, "microphones");
        cJSON_AddNumberToObject(item, "count", mics.mic_num);
        cJSON_AddBoolToObject(item, "echo_reference", mics.echo_ref_channel_index >= 0);
    }

    board_wifi_coprocessor_info_t coprocessor = {};
    if (board_wifi_get_coprocessor_info(&coprocessor) == ESP_OK) {
        cJSON *item = cJSON_AddObjectToObject(root, "wifi_coprocessor");
        cJSON_AddStringToObject(item, "chip", coprocessor.chip ? coprocessor.chip : "unknown");
        if (coprocessor.valid) {
            char version[24];
            std::snprintf(version, sizeof(version), "%lu.%lu.%lu",
                          static_cast<unsigned long>(coprocessor.major),
                          static_cast<unsigned long>(coprocessor.minor),
                          static_cast<unsigned long>(coprocessor.patch));
            cJSON_AddStringToObject(item, "firmware_version", version);
        }
    }
    return jsonValue(root);
}

void rebootTimerCallback(void *)
{
    esp_restart();
}

esp_mcp_value_t reboot(const esp_mcp_property_list_t *)
{
    std::lock_guard<std::mutex> lock(reboot_mutex);
    if (!reboot_timer) {
        esp_timer_create_args_t args = {};
        args.callback = rebootTimerCallback;
        args.name = "xiaozhi_reboot";
        if (esp_timer_create(&args, &reboot_timer) != ESP_OK) {
            reboot_timer = nullptr;
            return esp_mcp_value_create_bool(false);
        }
    }
    // A second request while the timer is pending keeps the first schedule.
    const esp_err_t ret = esp_timer_start_once(reboot_timer, REBOOT_DELAY_US);
    return esp_mcp_value_create_bool(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE);
}

esp_err_t addTool(esp_mcp_t *mcp, const char *name, const char *description,
                  esp_mcp_tool_callback_t callback)
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create(name, description, callback);
    if (!tool) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = esp_mcp_add_tool(mcp, tool);
    if (ret != ESP_OK) {
        esp_mcp_tool_destroy(tool);
    }
    return ret;
}

// Takes ownership of property, which may be NULL when its allocation failed.
esp_err_t addTool(esp_mcp_t *mcp, const char *name, const char *description,
                  esp_mcp_tool_callback_t callback, esp_mcp_property_t *property)
{
    if (!property) {
        return ESP_ERR_NO_MEM;
    }
    esp_mcp_tool_t *tool = esp_mcp_tool_create(name, description, callback);
    if (!tool) {
        esp_mcp_property_destroy(property);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = esp_mcp_tool_add_property(tool, property);
    if (ret != ESP_OK) {
        esp_mcp_property_destroy(property);
        esp_mcp_tool_destroy(tool);
        return ret;
    }
    ret = esp_mcp_add_tool(mcp, tool);
    if (ret != ESP_OK) {
        esp_mcp_tool_destroy(tool);
    }
    return ret;
}

} // namespace

void setXiaozhiDeveloperToolsChat(esp_xiaozhi_chat_handle_t chat)
{
    std::lock_guard<std::mutex> lock(camera_mutex);
    camera_chat = chat;
}

esp_err_t registerXiaozhiDeveloperTools(esp_mcp_t *mcp)
{
    if (!mcp) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = addTool(mcp, "device.audio.set_volume", "Set speaker volume from 0 to 95 and save it.", setVolume,
                            esp_mcp_property_create_with_range("volume", settings_storage::VOLUME_MIN,
                                                               settings_storage::VOLUME_MAX));
    if (ret != ESP_OK) return ret;
    ret = addTool(mcp, "device.audio.set_mute", "Mute (true) or unmute (false) the speaker.", setMute,
                  esp_mcp_property_create("mute", ESP_MCP_PROPERTY_TYPE_BOOLEAN));
    if (ret != ESP_OK) return ret;
    ret = addTool(mcp, "device.display.set_brightness", "Set display brightness from 20 to 100 and save it.",setBrightness,
                  esp_mcp_property_create_with_range("brightness", settings_storage::BRIGHTNESS_MIN,
                                                     settings_storage::BRIGHTNESS_MAX));
    if (ret != ESP_OK) return ret;
    ret = addTool(mcp, "device.display.set_screen","Turn the screen backlight on (true) or off (false). Turning it on restores the saved brightness.",setScreen, esp_mcp_property_create("on", ESP_MCP_PROPERTY_TYPE_BOOLEAN));
    if (ret != ESP_OK) return ret;
    ret = addTool(mcp, "device.get_status", "Get volume, display brightness and free memory in bytes.", getStatus);
    if (ret != ESP_OK) return ret;
    ret = addTool(mcp, "device.network.get_wifi_status","Get whether Wi-Fi is enabled, the link state, the SSID and the IPv4 address.", getWifiStatus);
    if (ret != ESP_OK) return ret;
    ret = addTool(mcp, "device.get_info","Get the chip, firmware and ESP-IDF versions, display size, microphones and Wi-Fi co-processor.",getDeviceInfo);
    if (ret != ESP_OK) return ret;
    ret = addTool(mcp, "device.system.reboot","Restart the device after a few seconds. Only call this when the user explicitly asks to restart.",reboot);
    if (ret != ESP_OK) return ret;
    esp_mcp_tool_t *tool = esp_mcp_tool_create("self.camera.explain_image", "Take a photo and answer a question about it.", explainImage);
    if (!tool) return ESP_ERR_NO_MEM;
    ret = esp_mcp_tool_set_task_support(tool, "forbidden");
    if (ret != ESP_OK) {
        esp_mcp_tool_destroy(tool);
        return ret;
    }
    esp_mcp_property_t *question = esp_mcp_property_create("question", ESP_MCP_PROPERTY_TYPE_STRING);
    if (!question) {
        esp_mcp_tool_destroy(tool);
        return ESP_ERR_NO_MEM;
    }
    ret = esp_mcp_tool_add_property(tool, question);
    if (ret != ESP_OK) {
        esp_mcp_property_destroy(question);
        esp_mcp_tool_destroy(tool);
        return ret;
    }
    ret = esp_mcp_add_tool(mcp, tool);
    if (ret != ESP_OK) esp_mcp_tool_destroy(tool);
    return ret;
}

} // namespace esp_brookesia::apps
