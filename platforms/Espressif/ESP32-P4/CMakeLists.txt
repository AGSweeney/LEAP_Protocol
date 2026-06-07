# LEAP device — Waveshare ESP32-P4-WIFI6-POE-ETH (ESP32-P4 + IP101)
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(leap_esp32_p4)

# Inject LEAP EtherType hook into lwIP (ESP-IDF 5.x uses ESP_IDF_LWIP_HOOK_FILENAME).
idf_component_get_property(lwip lwip COMPONENT_LIB)
target_compile_options(${lwip} PRIVATE "-I${CMAKE_SOURCE_DIR}/main")
target_compile_definitions(${lwip} PRIVATE "-DESP_IDF_LWIP_HOOK_FILENAME=\"lwip_hooks.h\"")
