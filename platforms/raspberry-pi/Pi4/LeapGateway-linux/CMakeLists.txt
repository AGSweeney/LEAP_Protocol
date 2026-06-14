cmake_minimum_required(VERSION 3.16)

project(LeapGatewayPi4Linux C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(LEAP_GATEWAY_OPENER_ENABLE "Link EtherNet/IP OpENer support" OFF)
set(LEAP_GATEWAY_OPENER_LIBRARY "" CACHE FILEPATH "Path to prebuilt OpENer library")
set(LEAP_GATEWAY_OPENER_INCLUDE_DIRS "" CACHE STRING "Semicolon-separated OpENer include directories")

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(GATEWAY_SHARED_DIR "${REPO_ROOT}/platforms/x86-32/D945GSEJT/LeapGateway")
set(GATEWAY_LINUX_DIR "${REPO_ROOT}/platforms/x86-32/D945GSEJT/LeapGateway-linux")

add_subdirectory("${REPO_ROOT}/leap_core" "${CMAKE_BINARY_DIR}/leap_core")

add_executable(leap-gateway
  "${GATEWAY_LINUX_DIR}/src/gateway_main_linux.c"
  "${GATEWAY_LINUX_DIR}/src/gateway_net_linux.c"
  "${GATEWAY_LINUX_DIR}/src/gateway_storage_linux.c"
  "${GATEWAY_LINUX_DIR}/src/leap_time_linux.c"
  "${GATEWAY_LINUX_DIR}/src/leap_transport_linux.c"
  "${GATEWAY_SHARED_DIR}/src/gateway_global.c"
  "${GATEWAY_SHARED_DIR}/src/gateway_http.c"
  "${GATEWAY_SHARED_DIR}/src/gateway_leap_session.c"
  "${GATEWAY_SHARED_DIR}/src/gateway_pd_io.c"
  "${GATEWAY_SHARED_DIR}/src/gateway_rtems_io.c"
  "${GATEWAY_SHARED_DIR}/src/gateway_web_index.c"
  "${GATEWAY_SHARED_DIR}/src/leap_gateway_opener.c"
)

target_include_directories(leap-gateway PRIVATE
  "${GATEWAY_SHARED_DIR}/src"
  "${GATEWAY_LINUX_DIR}/src"
)

target_compile_definitions(leap-gateway PRIVATE
  LEAP_GATEWAY_OPENER_ENABLE=$<BOOL:${LEAP_GATEWAY_OPENER_ENABLE}>
  _GNU_SOURCE
  _POSIX_C_SOURCE=200809L
)

target_link_libraries(leap-gateway PRIVATE leap_core pthread)

if(LEAP_GATEWAY_OPENER_ENABLE)
  if(NOT LEAP_GATEWAY_OPENER_LIBRARY)
    message(FATAL_ERROR "LEAP_GATEWAY_OPENER_LIBRARY is required when LEAP_GATEWAY_OPENER_ENABLE=ON")
  endif()
  target_include_directories(leap-gateway PRIVATE ${LEAP_GATEWAY_OPENER_INCLUDE_DIRS})
  target_link_libraries(leap-gateway PRIVATE "${LEAP_GATEWAY_OPENER_LIBRARY}")
endif()

if(NOT MSVC)
  target_compile_options(leap-gateway PRIVATE -Wall -Wextra -Wpedantic)
endif()
