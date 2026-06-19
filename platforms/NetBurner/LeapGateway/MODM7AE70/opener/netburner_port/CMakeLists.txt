enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

opener_common_includes()
opener_platform_support("INCLUDES")
add_subdirectory(leap_gateway)

set(PLATFORMLIBNAME ${OpENer_PLATFORM}PLATFORM)
set(PLATFORM_SPEC_SRC
  networkhandler.c
  opener_error.c
  networkconfig.c
  opener.c
  netburner_network_init.c
  netburner_stubs.c
  nb_eth_counters.c
)

add_library(${PLATFORMLIBNAME} ${PLATFORM_SPEC_SRC})

target_link_libraries(${PLATFORMLIBNAME}
  PLATFORM_GENERIC
  CIP
  ENET_ENCAP
  Utils
  NVDATA
  LeapGateway
  ${OpENer_CIP_OBJECTS}
)

if(NOT OpENer_TESTS)
  message(STATUS "NETBURNER platform configured as library-only (linked by LEAP Gateway).")
endif()
