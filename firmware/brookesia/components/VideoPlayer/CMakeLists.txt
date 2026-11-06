# CONFIGURE_DEPENDS re-checks the glob on every build, so newly added sources
# are compiled without a manual reconfigure. It is not allowed during the
# early requirement expansion, which runs this file in CMake script mode.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(GLOB_OPTIONS CONFIGURE_DEPENDS)
endif()
file(GLOB_RECURSE SRCS_C ${GLOB_OPTIONS} "${CMAKE_CURRENT_LIST_DIR}/*.c")
file(GLOB_RECURSE SRCS_CPP ${GLOB_OPTIONS} "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

# WHOLE_ARCHIVE keeps the static app registrar that main installs from the registry.
idf_component_register(
    SRCS ${SRCS_C} ${SRCS_CPP}
    INCLUDE_DIRS "."
    REQUIRES
        brookesia_core
        esp_extractor
        board
        driver
        esp_driver_jpeg
        esp_driver_ppa
        esp_lcd
        esp_lcd_touch
        esp_lvgl_adapter
        esp_new_jpeg
    PRIV_REQUIRES
        esp_mm
        esp_timer
    WHOLE_ARCHIVE
)

set_source_files_properties(${SRCS_CPP} PROPERTIES COMPILE_FLAGS "-Wno-missing-field-initializers")
