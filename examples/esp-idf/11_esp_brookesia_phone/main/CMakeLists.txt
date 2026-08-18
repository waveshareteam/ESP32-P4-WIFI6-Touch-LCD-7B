idf_component_register(
    SRCS main.cpp
    INCLUDE_DIRS .
    PRIV_REQUIRES
        apps
        product_audio
        esp-brookesia
        esp32_p4_wifi6_touch_lcd_7b
        freertos
        nvs_flash
        log
        esp_common
        esp_hw_support
        heap
        lvgl__lvgl
        esp_driver_sdmmc
        esp_driver_gpio
        esp_driver_isp)

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    if("espressif__esp_hosted" IN_LIST build_components AND "esp_driver_sdmmc" IN_LIST build_components)
        idf_component_get_property(esp_hosted_lib espressif__esp_hosted COMPONENT_LIB)
        idf_component_get_property(sdmmc_driver_lib esp_driver_sdmmc COMPONENT_LIB)
        # esp_hosted 1.4.x relied on the IDF 5 umbrella driver's transitive SDMMC include.
        target_link_libraries(${esp_hosted_lib} PRIVATE ${sdmmc_driver_lib})
    endif()

    if("espressif__esp_ipa" IN_LIST build_components AND "esp_driver_isp" IN_LIST build_components)
        idf_component_get_property(esp_ipa_lib espressif__esp_ipa COMPONENT_LIB)
        idf_component_get_property(isp_driver_lib esp_driver_isp COMPONENT_LIB)
        # esp_ipa 0.2 depended on the IDF 5 HAL path; IDF 6 needs esp_driver_isp
        # to publicly provide esp_hal_cam/hal/isp_types.h.
        target_link_libraries(${esp_ipa_lib} PUBLIC ${isp_driver_lib})
    endif()

    if("espressif__esp_video" IN_LIST build_components AND "esp_driver_gpio" IN_LIST build_components)
        idf_component_get_property(esp_video_lib espressif__esp_video COMPONENT_LIB)
        idf_component_get_property(gpio_driver_lib esp_driver_gpio COMPONENT_LIB)
        # IDF 6 provides driver/gpio.h through esp_driver_gpio.
        target_link_libraries(${esp_video_lib} PRIVATE ${gpio_driver_lib})
        # IDF 6 splits legacy generic YUV422 into explicit pack orders; UYVY matches
        # the ISP YUV422 contract, while SC2336 RAW/RGB565 paths remain unchanged.
        target_compile_definitions(${esp_video_lib} PRIVATE CAM_CTLR_COLOR_YUV422=CAM_CTLR_COLOR_YUV422_UYVY)
    endif()
endif()

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE 
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)

if(CONFIG_ESP_BROOKESIA_MEMORY_USE_CUSTOM)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "esp-brookesia" OR COMPONENT MATCHES "espressif__esp-brookesia")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component 'esp-brookesia' not found.")
    else()
        idf_component_get_property(ESP_BROOKESIA_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
    endif()

    target_compile_options(
        ${ESP_BROOKESIA_LIB}
        PUBLIC
            "-DESP_BROOKESIA_MEMORY_CUSTOM_MALLOC(x)=heap_caps_aligned_alloc(1, x, MALLOC_CAP_SPIRAM)"  "-DESP_BROOKESIA_MEMORY_CUSTOM_FREE=free"
    )
endif()

if(CONFIG_LV_MEM_CUSTOM)
    target_compile_options(
        ${LVGL_LIB} 
        PRIVATE 
            "-DLV_MEM_CUSTOM_ALLOC(x)=heap_caps_malloc(x, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)"
    )
endif()

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
