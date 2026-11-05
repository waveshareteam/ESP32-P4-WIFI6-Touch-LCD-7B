/* SPDX-License-Identifier: Apache-2.0 */

/*
 * esp_xiaozhi starts its WebSocket client without a task_stack, so the client
 * task gets the 4 KB default. MCP requests such as tools/list are parsed and
 * answered inside that task, and serializing the tool schemas overflows 4 KB.
 * The link option --wrap=esp_websocket_client_init (see CMakeLists.txt) routes
 * every call through here to raise the stack before the client is created.
 */

#include "esp_log.h"
#include "esp_websocket_client.h"

namespace {

constexpr char TAG[] = "XiaozhiWebsocket";
constexpr int MIN_WEBSOCKET_TASK_STACK = 8 * 1024;

} // namespace

extern "C" esp_websocket_client_handle_t __real_esp_websocket_client_init(
    const esp_websocket_client_config_t *config);

extern "C" esp_websocket_client_handle_t __wrap_esp_websocket_client_init(
    const esp_websocket_client_config_t *config)
{
    if (!config || config->task_stack >= MIN_WEBSOCKET_TASK_STACK) {
        return __real_esp_websocket_client_init(config);
    }
    esp_websocket_client_config_t patched = *config;
    patched.task_stack = MIN_WEBSOCKET_TASK_STACK;
    ESP_LOGI(TAG, "WebSocket task stack: %d bytes", patched.task_stack);
    return __real_esp_websocket_client_init(&patched);
}
