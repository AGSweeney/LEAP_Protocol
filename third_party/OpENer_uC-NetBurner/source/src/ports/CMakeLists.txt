#######################################
# OpENer shared port glue             #
#######################################

if(OPENER_BUILD_NVDATA)
  add_subdirectory( nvdata )
else()
  message(STATUS "NVDATA library excluded (OPENER_BUILD_NVDATA=OFF).")
endif()

opener_common_includes()
opener_platform_support("INCLUDES")

if(OPENER_BUILD_NETWORK_LAYER)
  set( PLATFORM_GENERIC_SRC generic_networkhandler.c socket_timer.c )
  add_library( PLATFORM_GENERIC ${PLATFORM_GENERIC_SRC} )
  target_link_libraries(PLATFORM_GENERIC OPENER_HAL)
else()
  add_library( PLATFORM_GENERIC socket_timer.c )
  target_link_libraries(PLATFORM_GENERIC OPENER_HAL)
  message(STATUS "Network layer disabled — generic_networkhandler.c excluded (core-only build).")
endif()

if( OPENER_INSTALL_AS_LIB )
  install(TARGETS PLATFORM_GENERIC
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  install(DIRECTORY ${PORTS_SRC_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING
      PATTERN "*.h"
      PATTERN "sample_application" EXCLUDE
   )
endif()
