if(CONFIG_IDF_TARGET_ESP32P4)
    idf_component_register(
        SRCS "src/bsp_extra_audio.c"
        INCLUDE_DIRS "include"
        REQUIRES esp_codec_dev esp32_p4_platform
    )
else()
    idf_component_register()
endif()
