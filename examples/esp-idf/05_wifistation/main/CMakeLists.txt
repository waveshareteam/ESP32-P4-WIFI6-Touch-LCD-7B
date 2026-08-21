idf_component_register(SRCS "station_example_main.c"
                    PRIV_REQUIRES esp_wifi esp_driver_sdmmc nvs_flash
                    INCLUDE_DIRS ".")

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    idf_component_get_property(esp_hosted_lib espressif__esp_hosted COMPONENT_LIB)
    idf_component_get_property(sdmmc_driver_lib esp_driver_sdmmc COMPONENT_LIB)
    # esp_hosted 1.4.x relied on the IDF 5 umbrella driver's transitive SDMMC include.
    target_link_libraries(${esp_hosted_lib} PRIVATE ${sdmmc_driver_lib})
endif()
