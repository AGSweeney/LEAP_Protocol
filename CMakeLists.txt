cmake_minimum_required(VERSION 3.16)
project(LEAP_Protocol C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(leap_core STATIC
  src/crc/leap_crc.c
  src/frame/leap_frame.c
  src/services/disc/leap_disc_device.c
  src/services/mgmt/leap_mgmt_device.c
  src/services/mgmt/leap_mgmt_process.c
  src/services/pd/leap_pd_device.c
  src/leap_device_stack.c
  src/transport/leap_raw_linux.c
)

target_include_directories(leap_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_executable(leap_tests
  tests/test_main.c
  tests/test_harness.c
  tests/test_crc.c
  tests/test_frame_vectors.c
  tests/test_frame_roundtrip.c
  tests/test_frame_fuzz.c
  tests/test_frame_fragment.c
  tests/test_mgmt_device.c
  tests/test_mgmt_boundaries.c
  tests/test_mgmt_process.c
  tests/test_pd_device.c
  tests/test_disc_device.c
  tests/test_device_stack.c
  tests/test_util.c
  tests/leap_test_frame.c
)

target_include_directories(leap_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(leap_tests PRIVATE leap_core)

enable_testing()
add_test(NAME leap_tests COMMAND leap_tests)

add_executable(device_minimal examples/device_minimal/main.c)
target_link_libraries(device_minimal PRIVATE leap_core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(leap_linux_device examples/linux_loopback/device_main.c)
  add_executable(leap_linux_controller examples/linux_loopback/controller_main.c)
  target_link_libraries(leap_linux_device PRIVATE leap_core)
  target_link_libraries(leap_linux_controller PRIVATE leap_core)
endif()
