set(SRCS_C
    "assets/img_app_xiaozhi.c"
)
set(SRCS_CPP
    "XiaozhiActivationClient.cpp"
    "XiaozhiActivationPrompt.cpp"
    "XiaozhiApp.cpp"
    "XiaozhiAudioProcessor.cpp"
    "XiaozhiClient.cpp"
    "XiaozhiDeveloperTools.cpp"
    "XiaozhiLocalization.cpp"
    "XiaozhiUi.cpp"
    "XiaozhiWebsocketStack.cpp"
    "third_party/xiaozhi_esp32/ogg_demuxer.cc"
)

# WHOLE_ARCHIVE keeps the static app registrar that main installs from the registry.
idf_component_register(
    SRCS ${SRCS_C} ${SRCS_CPP}
    INCLUDE_DIRS "."
    REQUIRES
        brookesia_core
        board
        esp-sr
        esp_audio_codec
        esp_audio_effects
        esp_event
        esp_netif
        esp_xiaozhi
        nvs_flash
        xiaozhi-fonts
    PRIV_REQUIRES
        app_update
        cjson
        efuse
        esp_app_format
        esp_http_client
        esp_hw_support
        esp_partition
        esp_timer
        esp_websocket_client
        esp_wifi
        esp_wifi_remote
        Camera
        Settings
        mbedtls
        mcp-c-sdk
        spi_flash
    WHOLE_ARCHIVE
)

set_source_files_properties(${SRCS_CPP} PROPERTIES COMPILE_FLAGS "-Wno-missing-field-initializers")

# mcp-c-sdk 2.0.1 (latest 2.0.1~1 included) bounds its strncpy() calls to
# sizeof(dst) - 1 on purpose, which GCC still reports as -Wstringop-truncation.
# Its target may be created after this component, so apply the option once the
# whole project has been configured.
if(TARGET __idf_espressif__mcp-c-sdk AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(__idf_espressif__mcp-c-sdk PRIVATE
        -Wno-error=stringop-truncation
    )
endif()

# esp_xiaozhi leaves the WebSocket task at the client's 4 KB default, which MCP
# tools/list overflows; XiaozhiWebsocketStack.cpp raises it through this wrap.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_websocket_client_init")
