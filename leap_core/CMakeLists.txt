cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LeapBuildInfo.cmake)
leap_configure_build_info(${CMAKE_BINARY_DIR}/generated)

add_library(leap_core STATIC
  src/crc/leap_crc.c
  src/frame/leap_frame.c
  src/services/disc/leap_disc_device.c
  src/services/disc/leap_disc_controller.c
  src/services/mgmt/leap_mgmt_device.c
  src/services/mgmt/leap_mgmt_process.c
  src/services/mgmt/leap_mgmt_controller.c
  src/services/dir/leap_dir_device.c
  src/services/dir/leap_dir_controller.c
  src/services/pd/leap_pd_device.c
  src/services/pd/leap_pd_common.c
  src/services/pd/leap_pd_controller.c
  src/leap_device_stack.c
  src/leap_controller_stack.c
  src/leap_controller_peer.c
  src/leap_controller_sequence.c
  src/leap_controller_session_hub.c
  src/leap_log.c
  src/leap_build_info.c
  src/services/diag/leap_diag_device.c
  src/services/diag/leap_diag_controller.c
  src/conformance/leap_conformance_engine.c
  src/conformance/leap_conformance_capabilities.c
  src/conformance/leap_conformance_scenario_digital_io.c
  src/conformance/leap_conformance_scenario_io_bench.c
  src/conformance/leap_conformance_scenarios.c
  src/services/dir/leap_dir_controller_capabilities.c
  src/conformance/leap_conformance_export.c
  src/conformance/leap_conformance_raw_io.c
  src/conformance/leap_commissioning.c
  src/bridge/leap_eip_bridge.c
  src/bridge/leap_gateway_config.c
)

if(WIN32)
  target_sources(leap_core PRIVATE src/transport/leap_raw_winpcap.c)
  target_sources(leap_core PRIVATE src/transport/leap_raw_linux.c)
  target_sources(leap_core PRIVATE src/platform/leap_win_time.c)
  target_link_libraries(leap_core PUBLIC iphlpapi ws2_32 winmm)
else()
  target_sources(leap_core PRIVATE src/transport/leap_raw_linux.c)
endif()

target_include_directories(leap_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
  ${CMAKE_BINARY_DIR}/generated
)

if(MSVC)
  target_compile_options(leap_core PRIVATE /W4)
else()
  target_compile_options(leap_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
