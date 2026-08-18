idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)

function(get_component_library component_name output_var)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    set(target_component "")
    foreach(component ${build_components})
        if(component MATCHES "${component_name}" OR component MATCHES "espressif__${component_name}")
            set(target_component ${component})
            break()
        endif()
    endforeach()

    if(target_component STREQUAL "")
        message(FATAL_ERROR "Component '${component_name}' not found.")
    endif()

    idf_component_get_property(component_lib ${target_component} COMPONENT_LIB)
    set(${output_var} ${component_lib} PARENT_SCOPE)
endfunction()

get_component_library("lvgl" LVGL_LIB)
target_compile_options(
    ${LVGL_LIB}
    PUBLIC
        -Wno-attributes
)

idf_build_get_property(build_components BUILD_COMPONENTS)
if("lvgl__lvgl" IN_LIST build_components)
    idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
    if(TARGET ${LVGL_LIB})
        target_compile_options(${LVGL_LIB} PRIVATE -Wno-error=attributes -Wno-error=cpp)
    endif()
endif()
