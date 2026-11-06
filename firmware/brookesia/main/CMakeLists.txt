# Pull in every project component. With MINIMAL_BUILD only components reachable
# from main are built, so a new app only needs its own directory under
# components/ plus an ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR registration.
file(GLOB PROJECT_COMPONENT_DIRS LIST_DIRECTORIES true "${CMAKE_CURRENT_LIST_DIR}/../components/*")
set(PROJECT_COMPONENTS "")
foreach(component_dir IN LISTS PROJECT_COMPONENT_DIRS)
    if(EXISTS "${component_dir}/CMakeLists.txt")
        get_filename_component(component_name "${component_dir}" NAME)
        list(APPEND PROJECT_COMPONENTS ${component_name})
    endif()
endforeach()

idf_component_register(
    SRCS "main.cpp" "BootLoading.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        ${PROJECT_COMPONENTS}
        esp_event
        esp_lvgl_adapter
        esp_netif
        esp_wifi
        nvs_flash
        board
)

# Copy the Xiaozhi UI font into the SPIFFS image source directory.
idf_component_get_property(XIAOZHI_FONTS_DIR 78__xiaozhi-fonts COMPONENT_DIR)
set(XIAOZHI_FONT_SRC "${XIAOZHI_FONTS_DIR}/cbin/font_puhui_common_30_4.bin")
if(XIAOZHI_FONTS_DIR AND EXISTS "${XIAOZHI_FONT_SRC}")
    configure_file("${XIAOZHI_FONT_SRC}" "${CMAKE_CURRENT_LIST_DIR}/../spiffs/xiaozhi/font.bin" COPYONLY)
else()
    message(STATUS "xiaozhi-fonts common font not found; skipping SPIFFS font copy")
endif()

spiffs_create_partition_image(storage "${CMAKE_CURRENT_LIST_DIR}/../spiffs" FLASH_IN_PROJECT)
