cmake_minimum_required(VERSION 3.16)

option(LEAP_BUILD_STUDIO_QT "Build LEAP Conformance Studio (Qt6)" OFF)

if(LEAP_BUILD_STUDIO_QT)
  project(LEAP_Protocol C CXX)
else()
  project(LEAP_Protocol C)
endif()

if(MSVC)
  set(CMAKE_C_STANDARD 11)
else()
  set(CMAKE_C_STANDARD 99)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(LEAP_BUILD_WIN_SMOKE "Build Windows Npcap wire smoke test" OFF)
option(LEAP_BUILD_WIN_L2 "Build Windows Npcap device/controller examples" OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(leap_core)
add_subdirectory(leap_cli)

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
  tests/test_pd_controller.c
  tests/test_pd_common.c
  tests/test_disc_device.c
  tests/test_disc_controller.c
  tests/test_dir_device.c
  tests/test_diag_device.c
  tests/test_diag_controller.c
  tests/test_controller_stack.c
  tests/test_controller_peer.c
  tests/test_controller_sequence.c
  tests/test_controller_session_hub.c
  tests/test_mgmt_controller.c
  tests/test_device_stack.c
  tests/test_comms_loss.c
  tests/test_raw_linux_stats.c
  tests/test_win_time.c
  tests/test_log.c
  tests/test_util.c
  tests/test_negative_frames.c
  tests/test_conformance_engine.c
  tests/test_eip_bridge.c
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
  add_executable(leap_linux_device
    examples/linux_loopback/device_main.c
    examples/linux_loopback/leap_linux_common.c
    examples/linux_loopback/leap_linux_io.c
    examples/linux_loopback/leap_linux_stats.c
  )
  add_executable(leap_linux_controller
    examples/linux_loopback/controller_main.c
    examples/linux_loopback/leap_linux_common.c
    examples/linux_loopback/leap_linux_pd.c
    examples/linux_loopback/leap_linux_controller_io.c
  )
  add_executable(leap_linux_discover
    examples/linux_loopback/discover_main.c
    examples/linux_loopback/leap_linux_common.c
    examples/linux_loopback/leap_linux_controller_io.c
  )
  add_executable(leap_linux_hub
    examples/linux_loopback/hub_main.c
    examples/linux_loopback/leap_linux_common.c
    examples/linux_loopback/leap_linux_pd.c
    examples/linux_loopback/leap_linux_controller_io.c
  )
  target_include_directories(leap_linux_device PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/linux_loopback
  )
  target_include_directories(leap_linux_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/linux_loopback
  )
  target_include_directories(leap_linux_discover PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/linux_loopback
  )
  target_include_directories(leap_linux_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/linux_loopback
  )
  target_link_libraries(leap_linux_device PRIVATE leap_core)
  target_link_libraries(leap_linux_controller PRIVATE leap_core)
  target_link_libraries(leap_linux_discover PRIVATE leap_core)
  target_link_libraries(leap_linux_hub PRIVATE leap_core)
endif()

if(LEAP_BUILD_STUDIO_QT)
  add_subdirectory(leap_studio_qt)
endif()
