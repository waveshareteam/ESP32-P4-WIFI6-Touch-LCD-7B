/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

ESP_EVENT_DEFINE_BASE(MONITOR_EVENT);

typedef enum {
    MONITOR_EVENT_SAMPLE = 1,
} monitor_event_id_t;

typedef struct {
    uint32_t sequence;
    int64_t uptime_ms;
    size_t free_heap;
    size_t min_free_heap;
    size_t free_psram;
} monitor_sample_t;

static const char *TAG = "system_monitor";
static const char *NVS_NAMESPACE = "monitor";
static const char *NVS_KEY_PERIOD = "period";
static const uint32_t DEFAULT_SAMPLE_PERIOD_MS = 2000;
static const uint32_t MIN_SAMPLE_PERIOD_MS = 500;
static const uint32_t MAX_SAMPLE_PERIOD_MS = 60000;

static QueueHandle_t s_sample_queue;
static nvs_handle_t s_nvs;
static uint32_t s_sample_period_ms = DEFAULT_SAMPLE_PERIOD_MS;

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs));
}

static void load_settings(void)
{
    uint32_t period = DEFAULT_SAMPLE_PERIOD_MS;

    esp_err_t err = nvs_get_u32(s_nvs, NVS_KEY_PERIOD, &period);
    if (err == ESP_OK && period >= MIN_SAMPLE_PERIOD_MS && period <= MAX_SAMPLE_PERIOD_MS) {
        s_sample_period_ms = period;
    } else {
        s_sample_period_ms = DEFAULT_SAMPLE_PERIOD_MS;
    }
}

static esp_err_t save_sample_period(uint32_t period_ms)
{
    if (period_ms < MIN_SAMPLE_PERIOD_MS || period_ms > MAX_SAMPLE_PERIOD_MS) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(nvs_set_u32(s_nvs, NVS_KEY_PERIOD, period_ms), TAG, "save period");
    ESP_RETURN_ON_ERROR(nvs_commit(s_nvs), TAG, "commit period");
    s_sample_period_ms = period_ms;
    return ESP_OK;
}

static monitor_sample_t make_sample(uint32_t sequence)
{
    monitor_sample_t sample = {
        .sequence = sequence,
        .uptime_ms = esp_timer_get_time() / 1000,
        .free_heap = esp_get_free_heap_size(),
        .min_free_heap = esp_get_minimum_free_heap_size(),
        .free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
    };

    return sample;
}

static void print_sample(const monitor_sample_t *sample)
{
    printf("sample=%" PRIu32 " uptime=%" PRId64 "ms heap=%u min_heap=%u psram=%u period=%ums\n",
           sample->sequence,
           sample->uptime_ms,
           (unsigned int)sample->free_heap,
           (unsigned int)sample->min_free_heap,
           (unsigned int)sample->free_psram,
           (unsigned int)s_sample_period_ms);
}

static void monitor_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    const monitor_sample_t *sample = (const monitor_sample_t *)event_data;

    ESP_LOGD(TAG, "event base=%s id=%" PRId32 " sample=%" PRIu32,
             base, id, sample->sequence);
}

static void sampler_task(void *arg)
{
    uint32_t sequence = 0;

    while (true) {
        monitor_sample_t sample = make_sample(sequence++);

        xQueueSend(s_sample_queue, &sample, pdMS_TO_TICKS(100));
        esp_event_post(MONITOR_EVENT, MONITOR_EVENT_SAMPLE, &sample, sizeof(sample), pdMS_TO_TICKS(100));

        vTaskDelay(pdMS_TO_TICKS(s_sample_period_ms));
    }
}

static void printer_task(void *arg)
{
    monitor_sample_t sample;

    while (true) {
        if (xQueueReceive(s_sample_queue, &sample, portMAX_DELAY) == pdPASS) {
            print_sample(&sample);
        }
    }
}

static int cmd_info(int argc, char **argv)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;

    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);

    printf("target: %s\n", CONFIG_IDF_TARGET);
    printf("cores: %d\n", chip_info.cores);
    printf("revision: v%d.%d\n", chip_info.revision / 100, chip_info.revision % 100);
    printf("flash: %u MB\n", (unsigned int)(flash_size / (1024 * 1024)));
    printf("psram: %s\n", esp_psram_is_initialized() ? "initialized" : "not initialized");
    printf("sample period: %" PRIu32 " ms\n", s_sample_period_ms);

    return 0;
}

static int cmd_heap(int argc, char **argv)
{
    monitor_sample_t sample = make_sample(0);
    print_sample(&sample);
    return 0;
}

static struct {
    struct arg_int *ms;
    struct arg_end *end;
} period_args;

static int cmd_period(int argc, char **argv)
{
    int errors = arg_parse(argc, argv, (void **)&period_args);
    if (errors != 0) {
        arg_print_errors(stderr, period_args.end, argv[0]);
        return 1;
    }

    uint32_t period_ms = (uint32_t)period_args.ms->ival[0];
    esp_err_t err = save_sample_period(period_ms);
    if (err != ESP_OK) {
        printf("period must be between %" PRIu32 " and %" PRIu32 " ms\n",
               MIN_SAMPLE_PERIOD_MS, MAX_SAMPLE_PERIOD_MS);
        return 1;
    }

    printf("sample period saved: %" PRIu32 " ms\n", s_sample_period_ms);
    return 0;
}

static void register_console_commands(void)
{
    const esp_console_cmd_t info_cmd = {
        .command = "info",
        .help = "Print chip, flash, PSRAM, and monitor settings",
        .hint = NULL,
        .func = &cmd_info,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&info_cmd));

    const esp_console_cmd_t heap_cmd = {
        .command = "heap",
        .help = "Print current heap and PSRAM usage",
        .hint = NULL,
        .func = &cmd_heap,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&heap_cmd));

    period_args.ms = arg_int1(NULL, NULL, "<ms>", "Sampling period in milliseconds");
    period_args.end = arg_end(2);
    const esp_console_cmd_t period_cmd = {
        .command = "period",
        .help = "Set and save the periodic sampling interval",
        .hint = NULL,
        .func = &cmd_period,
        .argtable = &period_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&period_cmd));
}

static void init_console(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = "monitor>";
    repl_config.max_cmdline_length = 256;

    esp_console_register_help_command();
    register_console_commands();

    printf("\nType 'help' to list commands. Try: info, heap, period 1000\n");

#if defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf(" ESP32-P4 System Monitor\n");
    printf("========================================\n");
    printf("Serial diagnostics with NVS settings, event loop, queue, and worker tasks.\n");
    printf("\n");

    init_nvs();
    load_settings();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(MONITOR_EVENT, ESP_EVENT_ANY_ID, monitor_event_handler, NULL));

    s_sample_queue = xQueueCreate(8, sizeof(monitor_sample_t));
    if (s_sample_queue == NULL) {
        ESP_LOGE(TAG, "failed to create sample queue");
        return;
    }

    init_console();

    xTaskCreate(sampler_task, "sampler", 3072, NULL, 5, NULL);
    xTaskCreate(printer_task, "printer", 3072, NULL, 5, NULL);
}
