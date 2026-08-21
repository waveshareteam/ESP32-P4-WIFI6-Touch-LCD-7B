set(SRCS "")
list(APPEND SRCS
    "src/product_audio.c"
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
    REQUIRES driver esp32_p4_wifi6_touch_lcd_7b
    PRIV_REQUIRES esp_timer fatfs esp_psram esp_mm
)
