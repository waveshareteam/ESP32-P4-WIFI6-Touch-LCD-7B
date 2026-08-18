#!/usr/bin/env python3
"""Host-side assertions for the boundary contracts changed in this review.

These checks intentionally avoid importing ESP-IDF, FreeRTOS, LVGL, or codec
libraries.  The firmware implementations are covered by the project builds;
this script checks the pure boundary rules and source-level invariants that can
be exercised on the host.
"""

from __future__ import annotations

from pathlib import Path
import json


ROOT = Path(__file__).resolve().parents[2]
USB_ROOT = ROOT / "examples/esp-idf/12_usb_extend_screen"
SETTING_FILES = (
    ROOT / "firmware/brookesia/components/apps/setting/Setting.cpp",
)
PRODUCT_AUDIO_FILES = (
    ROOT / "firmware/brookesia/components/product_audio/src/product_audio.c",
    USB_ROOT / "common_components/usb_extend_support/src/usb_extend_audio.c",
)
MP4_ADAPTER = ROOT / "examples/esp-idf/18_mp4_player/main/app_stream_adapter.c"
MP4_EXTRACTOR = ROOT / "examples/esp-idf/18_mp4_player/main/app_extractor.c"
USB_DESCRIPTORS = USB_ROOT / "main/tusb/usb_descriptors.c"
USB_MAIN_CMAKE = USB_ROOT / "main/CMakeLists.txt"
USB_MAIN_DIRECT_COMPONENT_INCLUDES = {
    "esp_timer": "esp_timer.h",
    "esp_driver_jpeg": "driver/jpeg_decode.h",
    "esp_driver_ppa": "driver/ppa.h",
    "esp_driver_gpio": "driver/gpio.h",
}
USB_MAIN_PHY_SOURCES = (
    USB_ROOT / "main/app_usb.c",
    USB_ROOT / "main/usb_frame.c",
)
USB_COMPONENT_CMAKE = USB_ROOT / "components/usb_device_uac/CMakeLists.txt"
USB_COMPONENT_SOURCE = USB_ROOT / "components/usb_device_uac/usb_device_uac.c"
USB_WORKFLOW = ROOT / ".github/workflows/esp-idf-examples.yml"
WIFI_MANIFEST = ROOT / "examples/esp-idf/05_wifistation/main/idf_component.yml"
WIFI_MAIN_CMAKE = ROOT / "examples/esp-idf/05_wifistation/main/CMakeLists.txt"
WIFI_READMES = (
    ROOT / "examples/esp-idf/05_wifistation/README.md",
    ROOT / "examples/esp-idf/05_wifistation/README_ZH.md",
)
CODEC_MANIFEST = ROOT / "examples/esp-idf/06_i2s_codec/main/idf_component.yml"
CODEC_MAIN_CMAKE = ROOT / "examples/esp-idf/06_i2s_codec/main/CMakeLists.txt"
CODEC_SOURCE = ROOT / "examples/esp-idf/06_i2s_codec/main/i2s_es8311_example.c"
CODEC_READMES = (
    ROOT / "examples/esp-idf/06_i2s_codec/README.md",
    ROOT / "examples/esp-idf/06_i2s_codec/README_ZH.md",
)
BROOKESIA_ROOT = ROOT / "examples/esp-idf/11_esp_brookesia_phone"
BROOKESIA_MANIFEST = BROOKESIA_ROOT / "main/idf_component.yml"
BROOKESIA_MAIN_CMAKE = BROOKESIA_ROOT / "main/CMakeLists.txt"
BROOKESIA_CORE_MANIFEST = BROOKESIA_ROOT / "components/brookesia_core/idf_component.yml"
BROOKESIA_APP_CMAKE = BROOKESIA_ROOT / "components/brookesia_app_squareline_demo/CMakeLists.txt"
BROOKESIA_APP_MANIFEST = BROOKESIA_ROOT / "components/brookesia_app_squareline_demo/idf_component.yml"
BROOKESIA_APP_SOURCE = BROOKESIA_ROOT / "components/brookesia_app_squareline_demo/esp_brookesia_app_squareline_demo.cpp"
BROOKESIA_READMES = (BROOKESIA_ROOT / "README.md", BROOKESIA_ROOT / "README_ZH.md")
BROOKESIA_LCD_COMPONENT_VERSION = 'version: "==2.0.2~1"'
BROOKESIA_LCD_LOCAL_OVERRIDE_COMPONENT_FILES = (
    BROOKESIA_ROOT / "components/esp_lcd_ek79007/idf_component.yml",
    BROOKESIA_ROOT / "components/esp_lcd_ek79007/CMakeLists.txt",
    BROOKESIA_ROOT / "components/esp_lcd_ek79007/include/esp_lcd_ek79007.h",
    BROOKESIA_ROOT / "components/esp_lcd_ek79007/esp_lcd_ek79007.c",
    ROOT / "firmware/brookesia/components/esp_lcd_ek79007/idf_component.yml",
    ROOT / "firmware/brookesia/components/esp_lcd_ek79007/CMakeLists.txt",
    ROOT / "firmware/brookesia/components/esp_lcd_ek79007/include/esp_lcd_ek79007.h",
    ROOT / "firmware/brookesia/components/esp_lcd_ek79007/esp_lcd_ek79007.c",
)
USB_SUPPORT_ROOT = USB_ROOT / "common_components/usb_extend_support"
USB_SUPPORT_CMAKE = USB_SUPPORT_ROOT / "CMakeLists.txt"
USB_SUPPORT_MANIFEST = USB_SUPPORT_ROOT / "idf_component.yml"
USB_MAIN_MANIFEST = USB_ROOT / "main/idf_component.yml"
USB_LCD_COMPONENT_VERSION = 'version: "==2.0.2~1"'
USB_READMES = (USB_ROOT / "README.md", USB_ROOT / "README_ZH.md")
USB_SUPPORT_PANEL = USB_SUPPORT_ROOT / "src/usb_extend_panel.c"
USB_SDKCONFIG_DEFAULTS = USB_ROOT / "sdkconfig.defaults.esp32p4"
USB_LVGL_PORTS = (
    USB_SUPPORT_ROOT / "src/usb_extend_lvgl_v9.c",
)
USB_APP_LCD = USB_ROOT / "main/app_lcd_p4.c"
USB_TOUCH_APP = USB_ROOT / "main/app_touch.c"
USB_TOUCH_LOCAL_OVERRIDE_COMPONENT_DIRS = (
    USB_ROOT / "components/espressif__esp_lcd_touch",
    USB_ROOT / "components/espressif__esp_lcd_touch_gt911",
    USB_ROOT / "components/espressif__esp_lcd_touch_ft5x06",
    USB_ROOT / "components/espressif__esp_lcd_touch_gt1151",
)


def copy_field(value: str | None, destination_size: int, max_length: int) -> bytes:
    if value is None or destination_size == 0:
        raise ValueError("invalid field")
    if len(value) > max_length or len(value) > destination_size:
        raise ValueError("field too long")
    return value.encode() + bytes(destination_size - len(value))


def validate_buffers(buffers: list[object] | None, count: int, size: int) -> None:
    if buffers is None or count == 0 or size == 0:
        raise ValueError("invalid buffer configuration")
    if len(buffers) < count or any(buffer is None for buffer in buffers[:count]):
        raise ValueError("invalid buffer configuration")


def validate_path(path: str | None, capacity: int = 128) -> None:
    if path is None or len(path) >= capacity:
        raise ValueError("invalid path")


def test_boundaries() -> None:
    assert len(copy_field("s" * 32, 32, 32)) == 32
    for value in ("s" * 33,):
        try:
            copy_field(value, 33, 32)
        except ValueError:
            pass
        else:
            raise AssertionError("33-byte SSID must be rejected")

    assert len(copy_field("p" * 63, 64, 63)) == 64
    try:
        copy_field("p" * 64, 64, 63)
    except ValueError:
        pass
    else:
        raise AssertionError("64-byte password must be rejected")

    validate_path("p" * 127)
    for path in ("p" * 128, "p" * 129, None):
        try:
            validate_path(path)
        except ValueError:
            pass
        else:
            raise AssertionError("path boundary must be rejected")

    old_state = (["old"], 1, 1024)
    try:
        validate_buffers(["new", None], 2, 1024)
    except ValueError:
        pass
    else:
        raise AssertionError("NULL resize buffer must be rejected")
    assert old_state == (["old"], 1, 1024)


def test_source_invariants() -> None:
    profiles = json.loads((ROOT / "config/esp32p4_revision_profiles.json").read_text(encoding="utf-8"))
    assert set(profiles["profiles"]) == {"rev1_3", "rev3_x"}
    assert profiles["profiles"]["rev1_3"]["arduino_chip_variant"] == "prev3"
    assert profiles["profiles"]["rev3_x"]["arduino_chip_variant"] == "postv3"
    assert not any((ROOT / "config" / name).exists() for name in ("esp32p4_rev_pre_v3.defaults", "esp32p4_rev_v3_0.defaults", "esp32p4_rev_v3_1.defaults"))
    assert "CONFIG_ESP32P4_REV_MIN_100=y" in (ROOT / "config/esp32p4_rev1_3.defaults").read_text(encoding="utf-8")
    assert "CONFIG_ESP32P4_REV_MIN_300=y" in (ROOT / "config/esp32p4_rev3_x.defaults").read_text(encoding="utf-8")
    for project in (ROOT / "examples/esp-idf").iterdir():
        if project.is_dir() and (project / "CMakeLists.txt").is_file():
            defaults = project / "sdkconfig.defaults"
            assert defaults.is_file(), defaults
            default_text = defaults.read_text(encoding="utf-8")
            assert "CONFIG_ESP32P4_REV_MIN_1=y" not in default_text
            assert default_text.count("CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y") == 1, defaults
            assert "CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y" not in default_text
            assert "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y" not in default_text
    usb_defaults = (USB_ROOT / "sdkconfig.defaults.esp32p4").read_text(encoding="utf-8")
    assert usb_defaults.count("CONFIG_ESP32P4_REV_MIN_100=y") == 1
    assert usb_defaults.count("CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y") == 1
    for profile in ("esp32p4_rev1_3.defaults", "esp32p4_rev3_x.defaults"):
        profile_text = (ROOT / "config" / profile).read_text(encoding="utf-8")
        assert profile_text.count("CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y") == 1
        assert "CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y" not in profile_text
        assert "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y" not in profile_text
    firmware_defaults = (ROOT / "firmware/brookesia/sdkconfig.defaults").read_text(encoding="utf-8")
    assert firmware_defaults.count("CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y") == 1
    assert "CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y" not in firmware_defaults
    assert "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y" not in firmware_defaults
    assert "CONFIG_PARTITION_TABLE_OFFSET=0x9000" in firmware_defaults
    partitions = (ROOT / "firmware/brookesia/partitions.csv").read_text(encoding="utf-8")
    assert "nvs,      data, nvs,     0xa000,  0x6000," in partitions
    assert "phy_init, data, phy,     0x10000, 0x1000," in partitions
    assert "automatically aligned to 0x20000" in partitions
    setting_texts = [path.read_text(encoding="utf-8") for path in SETTING_FILES]
    for text in setting_texts:
        assert "st_wifi_password" not in text
        assert "ESP_ERROR_CHECK(esp_wifi_disconnect" not in text
        assert "const bool psk_flag" in text
        assert text.count("resetWifiScanUi();") >= 4
        assert "WIFI_PASSWORD_MAX_LENGTH        (63)" in text
        assert "WIFI_SSID_MAX_LENGTH            (32)" in text
        assert "candidate_ssid[WIFI_SSID_MAX_LENGTH + 1]" in text
        assert "bsp_display_lock(1000)" in text
        assert "bsp_display_lock(pdMS_TO_TICKS(1000))" not in text
        assert "WIFI_EVENT_CONNECTED) &&" in text
        disconnected = text[text.index("WIFI_EVENT_STA_DISCONNECTED"):text.index("WIFI_EVENT_SCAN_DONE")]
        assert disconnected.index("memset(st_wifi_ssid") < disconnected.index("xEventGroupSetBits")

    for path in PRODUCT_AUDIO_FILES:
        text = path.read_text(encoding="utf-8")
        assert "if (bytes_read != NULL)" in text
        assert "if (bytes_written != NULL)" in text
        assert "(ret == ESP_OK) ? len : 0" in text
        assert "memcpy(audio_file_path, filename, sizeof(audio_file_path))" not in text
        assert "memcpy(audio_file_path, file_path, sizeof(audio_file_path))" not in text
        assert "strnlen(file_path, sizeof(audio_file_path))" in text

    adapter = MP4_ADAPTER.read_text(encoding="utf-8")
    extractor = MP4_EXTRACTOR.read_text(encoding="utf-8")
    assert "static esp_err_t stop_extract_task" in adapter
    assert "return ESP_ERR_TIMEOUT;" in adapter
    assert "static esp_err_t validate_decode_buffers" in adapter
    assert "buffer_count == 0 || buffer_size == 0" in adapter
    assert "decode_buffers[i] == NULL" in adapter
    assert "EXTRACT_TASK_STOPPED_BIT" not in adapter
    assert "xTaskNotify" not in adapter
    assert "ulTaskNotify" not in adapter
    assert "stop_waiter" not in adapter
    handle_pos = adapter.index("adapter->extract_task_handle = NULL;")
    release_pos = adapter.index("xSemaphoreGive(adapter->extract_state_mutex);", handle_pos)
    tail = adapter[release_pos + len("xSemaphoreGive(adapter->extract_state_mutex);"):
                   adapter.index("// Start extraction task", release_pos)]
    assert tail.strip().endswith("vTaskDelete(NULL);\n}")
    assert "xEventGroup" not in tail
    assert "adapter->" not in tail
    assert "extract_state_mutex" in adapter
    assert "task_finished &&" in adapter
    assert "EXTRACT_TASK_POLL_INTERVAL_MS" in adapter
    assert "operation_mutex" in adapter
    assert "xSemaphoreCreateBinary()" in adapter
    assert "xSemaphoreGive(adapter->operation_mutex);" in adapter
    assert "lock_operation" in adapter
    assert "Concurrent stream operation rejected" in adapter
    assert "stop_locked" in adapter
    assert "MP4_PLAYER_VIDEO_SYNC_ENABLED" not in extractor
    assert "last_video_pts" not in extractor
    assert "last_audio_pts" not in extractor

    descriptors = USB_DESCRIPTORS.read_text(encoding="utf-8")
    assert "#if CFG_TUD_AUDIO" in descriptors
    assert "TUD_AUDIO_DEVICE_DESC_LEN * CFG_TUD_AUDIO" in descriptors
    config_start = descriptors.index("#if CFG_TUD_AUDIO", descriptors.index("// Configuration Descriptor"))
    config_len = descriptors[config_start:descriptors.index("uint8_t const desc_fs_configuration", config_start)]
    assert "#else" in config_len
    assert config_len.count("TUD_AUDIO_DEVICE_DESC_LEN") == 1

    workflow = USB_WORKFLOW.read_text(encoding="utf-8")
    assert "command: |" in workflow
    assert "command: >-" not in workflow
    assert "config/esp32p4_rev1_3.defaults" in workflow
    assert 'idf.py -D "SDKCONFIG=${PWD}/build/${{ matrix.config_id }}/sdkconfig" -B "build/${{ matrix.config_id }}" set-target esp32p4 build' in workflow
    assert 'export SDKCONFIG="build/${{ matrix.config_id }}/sdkconfig"' not in workflow
    assert not (ROOT / ".github/workflows/product-firmware.yml").exists()

    main_cmake = USB_MAIN_CMAKE.read_text(encoding="utf-8")
    component_cmake = USB_COMPONENT_CMAKE.read_text(encoding="utf-8")
    component_source = USB_COMPONENT_SOURCE.read_text(encoding="utf-8")
    assert "if(CONFIG_UAC_AUDIO_ENABLE)" in main_cmake
    assert "set(requires usb_extend_support esp_timer esp_driver_jpeg esp_driver_ppa esp_driver_gpio)" in main_cmake
    assert "if(${IDF_VERSION_MAJOR} LESS 6)\n    list(APPEND requires usb)\nelse()\n    list(APPEND requires esp_hw_support)\nendif()" in main_cmake
    assert "list(APPEND requires usb_device_uac)" in main_cmake
    assert "PRIV_REQUIRES ${requires}" in main_cmake
    assert main_cmake.index("idf_component_register(") < main_cmake.index('target_include_directories(${COMPONENT_LIB} PRIVATE "${UAC_PATH}/include")')
    assert (
        '#include "usb_device_uac.h"' in (USB_ROOT / "main/app_uac.c").read_text(encoding="utf-8")
        and 'if(CONFIG_UAC_AUDIO_ENABLE)\n    list(APPEND srcs "app_uac.c")\nendif()' in main_cmake
        and 'if(CONFIG_UAC_AUDIO_ENABLE)\n    list(APPEND requires usb_device_uac)\nendif()' in main_cmake
        and 'if(CONFIG_UAC_AUDIO_ENABLE)\n    idf_component_get_property(UAC_PATH usb_device_uac COMPONENT_DIR)\n    target_include_directories(${COMPONENT_LIB} PRIVATE "${UAC_PATH}/include")\nendif()' in main_cmake
    )
    usb_main_source = USB_APP_LCD.read_text(encoding="utf-8")
    for component, include in USB_MAIN_DIRECT_COMPONENT_INCLUDES.items():
        assert component in main_cmake
        assert f'#include "{include}"' in usb_main_source
    assert "SRCS usb_device_uac.c" in component_cmake
    assert "PRIV_INCLUDE_DIRS \"tusb_uac\"" in component_cmake
    assert "if(${IDF_VERSION_MAJOR} LESS 6)\n    list(APPEND priv_requires usb)       # USB PHY is part of usb component in IDF < 6.0\nelse()\n    list(APPEND priv_requires esp_hw_support) # USB PHY moved to esp_hw_support in IDF >= 6.0\nendif()" in component_cmake
    assert "if(NOT CONFIG_USB_DEVICE_UAC_AS_PART)" in component_cmake
    assert "USB_DEVICE_UAC_APP_DISABLED" in component_cmake
    assert "#if defined(USB_DEVICE_UAC_APP_DISABLED)" in component_source
    assert '#include "esp_private/usb_phy.h"' in component_source
    for path in USB_MAIN_PHY_SOURCES:
        assert '#include "esp_private/usb_phy.h"' in path.read_text(encoding="utf-8")

    wifi_manifest = WIFI_MANIFEST.read_text(encoding="utf-8")
    assert 'version: "==1.2.5"' in wifi_manifest
    assert 'version: "1.4.*"' in wifi_manifest
    assert "esp_interface.h" in wifi_manifest
    assert "esp_hosted 1.4.*" in wifi_manifest
    wifi_cmake = WIFI_MAIN_CMAKE.read_text(encoding="utf-8")
    assert "PRIV_REQUIRES esp_wifi esp_driver_sdmmc nvs_flash" in wifi_cmake
    assert "if(IDF_VERSION_MAJOR GREATER_EQUAL 6)" in wifi_cmake
    assert "idf_component_get_property(esp_hosted_lib espressif__esp_hosted COMPONENT_LIB)" in wifi_cmake
    assert "idf_component_get_property(sdmmc_driver_lib esp_driver_sdmmc COMPONENT_LIB)" in wifi_cmake
    assert "target_link_libraries(${esp_hosted_lib} PRIVATE ${sdmmc_driver_lib})" in wifi_cmake
    assert "transitive SDMMC include" in wifi_cmake

    codec_manifest = CODEC_MANIFEST.read_text(encoding="utf-8")
    codec_cmake = CODEC_MAIN_CMAKE.read_text(encoding="utf-8")
    codec_source = CODEC_SOURCE.read_text(encoding="utf-8")
    codec_kconfig = (CODEC_SOURCE.parent / "Kconfig.projbuild").read_text(encoding="utf-8")
    assert 'version: "==3.0.0"' in codec_manifest
    assert "espressif/es8311" not in codec_manifest
    assert "espressif/esp_lvgl_port" not in codec_manifest
    assert "PRIV_REQUIRES" not in codec_cmake
    assert "IDF_VERSION_MAJOR" not in codec_cmake
    assert "bsp_audio_codec_speaker_init()" in codec_source
    assert "bsp_audio_codec_microphone_init()" in codec_source
    assert "esp_codec_dev_open" in codec_source
    assert "esp_codec_dev_write" in codec_source
    assert '#include "driver/gpio.h"' not in codec_source
    assert "CONFIG_EXAMPLE_BSP" not in codec_source + codec_kconfig
    assert "config EXAMPLE_MODE_ECHO" in codec_kconfig
    assert "config EXAMPLE_MIC_GAIN_DB" in codec_kconfig

    wifi_readme, wifi_readme_zh = (path.read_text(encoding="utf-8") for path in WIFI_READMES)
    wifi_readme_normalized = " ".join(wifi_readme.split())
    assert "ESP-IDF v5.5.5 and v6.0.2" in wifi_readme
    assert "C6/ESP-Hosted" in wifi_readme
    assert "compatibility pin and compile shim preserve source coverage" in wifi_readme_normalized
    assert "do not verify the existing coprocessor firmware or hardware runtime" in wifi_readme_normalized
    assert "ESP-IDF v5.5.5 和 v6.0.2" in wifi_readme_zh
    assert "C6/ESP-Hosted 运行边界" in wifi_readme_zh
    assert "兼容固定和编译垫片仅保留源码覆盖" in wifi_readme_zh
    assert "尚未验证现有协处理器固件或硬件运行时" in wifi_readme_zh

    codec_readme, codec_readme_zh = (path.read_text(encoding="utf-8") for path in CODEC_READMES)
    assert "ESP-IDF v5.5.5 and v6.0.2" in codec_readme
    assert "managed BSP" in codec_readme
    assert "`esp_codec_dev`" in codec_readme
    assert "Compilation does not verify audio runtime behavior" in codec_readme
    assert "ESP-IDF v5.5.5 和 v6.0.2" in codec_readme_zh
    assert "管理型开发板 BSP" in codec_readme_zh
    assert "`esp_codec_dev`" in codec_readme_zh
    assert "编译不代表已在硬件上验证音频运行行为" in codec_readme_zh

    brookesia_manifest = BROOKESIA_MANIFEST.read_text(encoding="utf-8")
    brookesia_main_cmake = BROOKESIA_MAIN_CMAKE.read_text(encoding="utf-8")
    brookesia_core_manifest = BROOKESIA_CORE_MANIFEST.read_text(encoding="utf-8")
    brookesia_app_cmake = BROOKESIA_APP_CMAKE.read_text(encoding="utf-8")
    brookesia_app_manifest = BROOKESIA_APP_MANIFEST.read_text(encoding="utf-8")
    brookesia_app_source = BROOKESIA_APP_SOURCE.read_text(encoding="utf-8")

    assert 'idf: ">=5.5"' in brookesia_manifest
    assert 'waveshare/esp32_p4_wifi6_touch_lcd_7b:\n    version: "==3.0.0"' in brookesia_manifest
    assert "espressif/esp-brookesia" not in brookesia_manifest
    assert 'SRCS "main.cpp"' in brookesia_main_cmake
    assert "get_component_library(\"lvgl\" LVGL_LIB)" in brookesia_main_cmake

    assert 'version: "0.6.0-beta2"' in brookesia_core_manifest
    assert 'lvgl/lvgl:' in brookesia_core_manifest
    assert 'version: "9.5.0"' in brookesia_core_manifest
    assert 'version: 0.6.0' in brookesia_app_manifest
    assert "brookesia_core" in brookesia_app_manifest
    assert "WHOLE_ARCHIVE" in brookesia_app_cmake
    assert "ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR" in brookesia_app_source
    assert "phone_app_squareline_ui_init" in brookesia_app_source
    assert not (BROOKESIA_ROOT / "components/apps").exists()
    assert not (BROOKESIA_ROOT / "components/product_audio").exists()
    assert not (BROOKESIA_ROOT / "spiffs").exists()
    assert all(not path.exists() for path in BROOKESIA_LCD_LOCAL_OVERRIDE_COMPONENT_FILES)

    usb_main_cmake = USB_MAIN_CMAKE.read_text(encoding="utf-8")
    usb_support_cmake = USB_SUPPORT_CMAKE.read_text(encoding="utf-8")
    usb_main_manifest = USB_MAIN_MANIFEST.read_text(encoding="utf-8")
    usb_support_manifest = USB_SUPPORT_MANIFEST.read_text(encoding="utf-8")
    assert "list(APPEND requires esp_driver_i2s)" not in usb_main_cmake
    assert '"chmorgan__esp-audio-player" IN_LIST build_components' not in usb_main_cmake
    assert "target_link_libraries(${audio_player_lib} PRIVATE ${i2s_driver_lib})" not in usb_main_cmake
    assert 'version: "==1.1.0"' in usb_support_manifest
    assert "version: '>=5.3.0'" in usb_support_manifest
    assert USB_LCD_COMPONENT_VERSION in usb_main_manifest
    assert USB_LCD_COMPONENT_VERSION in usb_support_manifest
    assert 'espressif/esp_lcd_ek79007:\n    version: "*"' not in usb_main_manifest
    assert 'espressif/esp_lcd_ek79007:\n    version: "*"' not in usb_support_manifest
    assert 'version: "^0.1.0"' not in usb_support_manifest
    assert "natively supports the IDF 6 MIPI/DMA2D layout" in usb_support_manifest
    assert "keeps the dual matrix reproducible" in usb_support_manifest
    assert 'espressif/esp_lcd_touch:\n    # Keep the custom USB/LVGL touch bridge reproducible on both supported IDF lines.\n    version: "==1.2.1"\n    public: true' in usb_support_manifest
    assert 'espressif/esp_lcd_touch_gt911:\n    version: "==1.2.0~3"\n    public: true' in usb_support_manifest
    assert 'espressif/esp_lcd_touch_ft5x06:\n    version: "==1.1.0~2"\n    public: true' in usb_support_manifest
    assert all(not path.exists() for path in USB_TOUCH_LOCAL_OVERRIDE_COMPONENT_DIRS)
    assert 'waveshare/esp32_p4_wifi6_touch_lcd_7b:\n    version: "==3.0.0"' in usb_main_manifest
    assert 'waveshare/esp32_p4_wifi6_touch_lcd_7b:\n    # The extension composes the managed board APIs; it does not replace the board BSP.\n    version: "==3.0.0"' in usb_support_manifest
    assert "espressif/esp_lvgl_port" not in usb_main_manifest + usb_support_manifest
    assert 'usb_extend_support:\n    override_path: "../common_components/usb_extend_support"' in usb_main_manifest
    for dependency in ("driver", "esp_driver_i2s", "esp_driver_i2c", "esp_driver_sdmmc", "esp_driver_gpio", "esp_driver_ledc"):
        assert dependency in usb_support_cmake
    for dependency in ("heap", "esp_hw_support", "esp_common"):
        assert dependency in usb_support_cmake
    assert "esp_driver_dma" not in usb_support_cmake

    usb_panel = USB_SUPPORT_PANEL.read_text(encoding="utf-8")
    for old_symbol in (
        "bsp_ldo_power_on",
        "EK79007_PANEL_BUS_DSI_2CH_CONFIG",
        "EK79007_PANEL_IO_DBI_CONFIG",
        "esp_lcd_new_panel_ek79007",
        "gpio_set_level(33, 1)",
    ):
        assert old_symbol not in usb_panel
    assert "bsp_display_new_with_handles(NULL, &lcd_handles)" in usb_panel
    assert "bsp_lcd_handles_t lcd_handles = {0};" in usb_panel
    assert "mipi_dsi_bus = lcd_handles.mipi_dsi_bus;" in usb_panel
    assert "mipi_dbi_io = lcd_handles.io;" in usb_panel
    assert "mipi_dpi_panel = lcd_handles.panel;" in usb_panel
    assert "config == NULL || config->dpi_fb_buf_num == CONFIG_BSP_LCD_DPI_BUFFER_NUMS" in usb_panel
    assert "CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3" in USB_SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
    assert "CONFIG_EXAMPLE_LCD_BUF_COUNT=3" in USB_SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
    assert "bsp_i2c_get_handle();" in usb_panel
    assert "i2c_bus_handle != NULL" in usb_panel
    assert ".mirror_x = 1," in usb_panel
    assert ".mirror_y = 1," in usb_panel
    assert "usb_extend_display_new" in usb_panel
    assert "usb_extend_touch_new" in usb_panel

    for path in USB_LVGL_PORTS:
        text = path.read_text(encoding="utf-8")
        assert '#include "esp_idf_version.h"' in text
        assert "#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)" in text
        assert '#include "esp_dma_utils.h"' in text
        assert "heap_caps_calloc(1, buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED)" in text
        assert "esp_dma_capable_calloc(1, buffer_size, &dma_mem_info, &buf1, &buffer_size)" in text
    assert '#include "esp_dma_utils.h"' not in USB_APP_LCD.read_text(encoding="utf-8")

    # Online-BSP ownership contracts: no local component may reuse the managed
    # board identity, and every adapter explicitly pins the same BSP release.
    local_bsp_component_dirs = (
        ROOT / "examples/esp-idf/08_lvgl_display_panel/components/bsp_extra",
        ROOT / "examples/esp-idf/09_lvgl_demo_v9/components/bsp_extra",
        BROOKESIA_ROOT / "components/bsp_extra",
        ROOT / "firmware/brookesia/components/bsp_extra",
    )
    assert all(not path.exists() for path in local_bsp_component_dirs)
    assert not (USB_ROOT / "common_components/esp32_p4_wifi6_touch_lcd_7b_usb_bsp").exists()
    assert not (ROOT / "components/product_display").exists()
    first_party_code = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in ROOT.rglob("*")
        if path.is_file()
        and (path.suffix in {".c", ".cc", ".cpp", ".h", ".hpp"} or path.name == "CMakeLists.txt")
        and not {".git", "build", "managed_components"}.intersection(path.parts)
    )
    for direct_adapter_api in (
        "esp_lv_adapter_init(",
        "esp_lv_adapter_register_display(",
        "esp_lv_adapter_register_touch(",
        "esp_lv_adapter_start(",
        "esp_lv_adapter_lock(",
        "esp_lv_adapter_unlock(",
        "BSP_CONFIG_NO_GRAPHIC_LIB",
    ):
        assert direct_adapter_api not in first_party_code
    managed_bsp_manifests = tuple(
        path for path in ROOT.rglob("idf_component.yml")
        if "waveshare/esp32_p4_wifi6_touch_lcd_7b:" in path.read_text(encoding="utf-8")
    )
    assert {path.relative_to(ROOT).as_posix() for path in managed_bsp_manifests} == {
        "examples/esp-idf/06_i2s_codec/main/idf_component.yml",
        "examples/esp-idf/08_lvgl_display_panel/main/idf_component.yml",
        "examples/esp-idf/09_lvgl_demo_v9/main/idf_component.yml",
        "examples/esp-idf/11_esp_brookesia_phone/main/idf_component.yml",
        "examples/esp-idf/12_usb_extend_screen/common_components/usb_extend_support/idf_component.yml",
        "examples/esp-idf/12_usb_extend_screen/main/idf_component.yml",
        "examples/esp-idf/13_rs485_test/main/idf_component.yml",
        "examples/esp-idf/18_mp4_player/main/idf_component.yml",
        "firmware/brookesia/components/product_audio/idf_component.yml",
    }
    for manifest in managed_bsp_manifests:
        text = manifest.read_text(encoding="utf-8")
        assert "waveshare/esp32_p4_wifi6_touch_lcd_7b:" in text
        assert 'version: "==3.0.0"' in text
        assert 'version: "==2.0.0"' not in text
        assert "espressif/esp_lvgl_port:" not in text
        bsp_contract = text.split("waveshare/esp32_p4_wifi6_touch_lcd_7b:", 1)[1].split("\n  ", 1)[0]
        assert "override_path" not in bsp_contract

    display_cfg_files = (
        ROOT / "examples/esp-idf/08_lvgl_display_panel/main/main.c",
        ROOT / "examples/esp-idf/09_lvgl_demo_v9/main/main.c",
        BROOKESIA_ROOT / "main/main.cpp",
        ROOT / "firmware/brookesia/main/main.cpp",
    )
    for path in display_cfg_files:
        text = path.read_text(encoding="utf-8")
        assert '#include "product_display.h"' not in text
        assert "product_display_" not in text
        assert "bsp_display_cfg_t cfg" in text
        assert "ESP_LV_ADAPTER_DEFAULT_CONFIG()" in text
        assert "bsp_display_start_with_config(&cfg)" in text
        assert "lvgl_port_cfg" not in text
        assert "BSP_LCD_DRAW_BUFF" not in text
    for path in display_cfg_files[:2]:
        assert "ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL" in path.read_text(encoding="utf-8")
    for path in display_cfg_files[2:]:
        assert "ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT" in path.read_text(encoding="utf-8")
    assert "lv_task_handler()" not in display_cfg_files[0].read_text(encoding="utf-8")

    for defaults in (
        ROOT / "examples/esp-idf/08_lvgl_display_panel/sdkconfig.defaults",
        ROOT / "examples/esp-idf/09_lvgl_demo_v9/sdkconfig.defaults",
    ):
        text = defaults.read_text(encoding="utf-8")
        assert "# CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM is not set" in text
        assert "CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y" not in text
        assert "CONFIG_LV_USE_DEMO_SCROLL" not in text
        assert "CONFIG_LV_USE_DEMO_TRANSFORM" not in text

    for project in ("08_lvgl_display_panel", "09_lvgl_demo_v9"):
        demo_cmake = (ROOT / "examples/esp-idf" / project / "main/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        assert "managed_components" not in demo_cmake
        assert "GLOB_RECURSE" not in demo_cmake
        assert "REQUIRES esp32_p4_wifi6_touch_lcd_7b lvgl__lvgl" in demo_cmake

    for defaults in (
        BROOKESIA_ROOT / "sdkconfig.defaults",
        ROOT / "firmware/brookesia/sdkconfig.defaults",
    ):
        text = defaults.read_text(encoding="utf-8")
        assert "CONFIG_BSP_LCD_DPI_BUFFER_NUMS=2" in text
        assert "CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR" not in text
        assert "CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE" not in text

    lock_contract_files = display_cfg_files + SETTING_FILES + (
        ROOT / "firmware/brookesia/components/apps/video_player/esp_lvgl_simple_player/esp_lvgl_simple_player.c",
    )
    for path in lock_contract_files:
        text = path.read_text(encoding="utf-8")
        assert "bsp_display_lock(" in text, path
        assert "bsp_display_unlock(" in text, path
        assert "product_display_" not in text, path

    source_boundary_files = tuple(
        path
        for path in USB_ROOT.glob("**/*")
        if path.suffix in {".c", ".h"}
        and not {"build", "managed_components"}.intersection(path.relative_to(USB_ROOT).parts)
    )
    source_boundary_text = "\n".join(path.read_text(encoding="utf-8") for path in source_boundary_files)
    assert "bsp_board_extra.h" not in source_boundary_text
    assert "bsp_extra" not in source_boundary_text
    assert "esp32_p4_wifi6_touch_lcd_7b_usb_bsp" not in source_boundary_text
    assert '"bsp/esp-bsp.h"' in USB_SUPPORT_PANEL.read_text(encoding="utf-8")
    usb_app_lcd = USB_APP_LCD.read_text(encoding="utf-8")
    assert "usb_extend_display_new" in usb_app_lcd
    assert "bsp_display_brightness_init" not in usb_app_lcd
    assert "bsp_display_backlight_on" in usb_app_lcd
    assert "usb_extend_touch_new" in USB_TOUCH_APP.read_text(encoding="utf-8")
    for path in (USB_TOUCH_APP,) + USB_LVGL_PORTS:
        text = path.read_text(encoding="utf-8")
        assert "esp_lcd_touch_get_coordinates" not in text
        assert "esp_lcd_touch_get_data" in text
        assert "esp_lcd_touch_point_data_t" in text
    for path in USB_LVGL_PORTS:
        text = path.read_text(encoding="utf-8")
        assert "usb_extend_lvgl_" in text
        assert "usb_extend_display_register_callback" in text

    assert "bool is_gt911 = false;" in usb_panel
    assert usb_panel.count("is_gt911 = true;") == 2
    assert "if (is_gt911) {\n        esp_lcd_touch_io_gt911_config_t tp_gt911_config" in usb_panel
    assert ".driver_data = &tp_gt911_config;\n        return esp_lcd_touch_new_i2c_gt911" in usb_panel
    assert "return esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch);" in usb_panel
    usb_lvgl_v9 = USB_LVGL_PORTS[0].read_text(encoding="utf-8")
    assert "lv_indev_get_user_data(indev_drv)" in usb_lvgl_v9
    assert "lv_indev_get_user_data(indev);" not in usb_lvgl_v9

    brookesia_readme, brookesia_readme_zh = (path.read_text(encoding="utf-8") for path in BROOKESIA_READMES)
    brookesia_readme_normalized = " ".join(brookesia_readme.split())
    brookesia_readme_zh_normalized = " ".join(brookesia_readme_zh.split())
    assert "LVGL 9" in brookesia_readme_normalized
    assert "brookesia_app_squareline_demo" in brookesia_readme_normalized
    assert "ESP-Brookesia 0.4.2" not in brookesia_readme_normalized
    assert "SPIFFS" not in brookesia_readme_normalized
    assert "LVGL 9" in brookesia_readme_zh_normalized
    assert "brookesia_app_squareline_demo" in brookesia_readme_zh_normalized
    assert "ESP-Brookesia 0.4.2" not in brookesia_readme_zh_normalized
    assert "SPIFFS" not in brookesia_readme_zh_normalized

    usb_readme, usb_readme_zh = (path.read_text(encoding="utf-8") for path in USB_READMES)
    usb_readme_normalized = " ".join(usb_readme.split())
    usb_readme_zh_normalized = " ".join(usb_readme_zh.split())
    assert "ESP-IDF v5.5.5 and v6.0.2" in usb_readme
    assert "GitHub Actions verifies compilation only; it does not verify hardware runtime." in usb_readme_normalized
    assert "does not build the bundled firmware or ESP-Brookesia separately." in usb_readme_normalized
    assert "ESP-IDF v5.5.5 和 v6.0.2" in usb_readme_zh
    assert "GitHub Actions 仅验证编译，不验证硬件运行。" in usb_readme_zh_normalized
    assert "不单独构建随附的 firmware 或 ESP-Brookesia。" in usb_readme_zh_normalized
    assert "EK79007 version-gated API plus the IDF 6 allocator/DMA2D path" in usb_readme
    assert "do not verify LCD runtime behavior" in usb_readme
    assert "EK79007 版本守卫 API 与 IDF 6 分配器/DMA2D 路径" in usb_readme_zh
    assert "不验证 LCD 运行行为" in usb_readme_zh

    for text in setting_texts:
        callback_start = text.index("onButtonWifiListClickedEventCallback")
        callback_end = text.index("void AppSettings::", callback_start + 1)
        callback = text[callback_start:callback_end]
        assert "app->stopWifiScan()" not in callback
        reset = text[text.index("void AppSettings::resetWifiScanUi"):]
        assert "if (!bsp_display_lock(-1))" in reset


def main() -> int:
    test_boundaries()
    test_source_invariants()
    print("Review boundary/source assertions passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
