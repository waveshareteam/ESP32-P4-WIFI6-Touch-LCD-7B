set(SRCS "")
list(APPEND SRCS
    "src/board_camera.c"
    "src/board_codec.c"
    "src/board_audio.c"
    "src/board_display.c"
    "src/board_peripherals.c"
    "src/board_storage.c"
    "src/board_wifi.c"
)

set(INCLUDE_DIRS "")
list(APPEND INCLUDE_DIRS "include")

foreach(SRC_DIR IN LISTS SRC_DIRS)
    file(GLOB_RECURSE SRC ${SRC_DIR}/*.c)
    list(APPEND SRCS ${SRC})
    list(APPEND INCLUDE_DIRS ${SRC_DIR}/include)
endforeach()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES driver esp_lcd_touch esp_lvgl_adapter esp_board_manager
    PRIV_REQUIRES esp_timer fatfs esp_psram esp_mm esp_driver_ledc esp_driver_uart esp_lcd_touch_gt911
)

target_compile_options(
    ${COMPONENT_LIB} PRIVATE
        -Wno-format
        -Wno-int-conversion
        -Wno-incompatible-pointer-types
        -Wunused-function
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-overflow
        -Wno-unused-but-set-variable
        -Wno-discarded-qualifiers)
