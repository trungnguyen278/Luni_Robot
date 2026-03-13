# ESP32-S3 main component

FILE(GLOB_RECURSE app_sources
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.c
)

set(app_includes
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/config
    ${CMAKE_SOURCE_DIR}/src/system
    ${CMAKE_SOURCE_DIR}/src/assets
    ${CMAKE_SOURCE_DIR}/lib/display
    ${CMAKE_SOURCE_DIR}/lib/power
    ${CMAKE_SOURCE_DIR}/lib/uart
)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS ${app_includes}
    REQUIRES
        driver
        esp_adc
        nvs_flash
        spi_flash
        freertos
        esp_timer
)
