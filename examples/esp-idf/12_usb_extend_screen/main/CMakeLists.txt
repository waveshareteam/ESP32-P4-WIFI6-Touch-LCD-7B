set(srcs "app_hid.c" "app_usb.c" "app_vendor.c"
         "usb_extend_screen.c" "usb_frame.c" "app_lcd_p4.c")

if(CONFIG_HID_TOUCH_ENABLE)
    list (APPEND srcs "app_touch.c")
endif()

if(CONFIG_UAC_AUDIO_ENABLE)
    list(APPEND srcs "app_uac.c")
endif()

set(requires usb_extend_support esp_timer esp_driver_jpeg esp_driver_ppa esp_driver_gpio)
if(${IDF_VERSION_MAJOR} LESS 6)
    list(APPEND requires usb)
else()
    list(APPEND requires esp_hw_support)
endif()
if(CONFIG_UAC_AUDIO_ENABLE)
    list(APPEND requires usb_device_uac)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include" "tusb"
                       PRIV_REQUIRES ${requires})

if(CONFIG_UAC_AUDIO_ENABLE)
    idf_component_get_property(UAC_PATH usb_device_uac COMPONENT_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${UAC_PATH}/include")
endif()

# Determine whether tinyusb is fetched from component registry or from local path
idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
    set(tinyusb_name tinyusb) # Local component
else()
    set(tinyusb_name espressif__tinyusb) # Managed component
endif()

idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${PROJECT_DIR}/main/tusb")
if(CONFIG_UAC_AUDIO_ENABLE)
    target_include_directories(${tusb_lib} PUBLIC "${UAC_PATH}/tusb_uac")
endif()
target_sources(${tusb_lib} PUBLIC "${PROJECT_DIR}/main/tusb/usb_descriptors.c")

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
