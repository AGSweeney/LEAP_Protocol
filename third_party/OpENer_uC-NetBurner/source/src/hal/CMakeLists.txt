#######################################
# OpENer embedded HAL library         #
#######################################

set(HAL_SRC
  opener_mem.c
  opener_error.c
  opener_cip_alloc.c
)

if(OPENER_NET_BACKEND STREQUAL "stub")
  list(APPEND HAL_SRC opener_hal_stub.c)
elseif(OPENER_NET_BACKEND STREQUAL "netburner")
  enable_language(CXX)
  list(APPEND HAL_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_hal_netburner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_nb_ifconfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_hal_timer_netburner.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_nb_identity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_nb_fault.c
  )
  if(OPENER_NB_LLDP)
    list(APPEND HAL_SRC
      ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_nb_lldp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/nndk_overload/lldp.cpp
    )
  endif()
  if(OPENER_NB_ACD)
    list(APPEND HAL_SRC
      ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_nb_acd.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/opener_nb_nv.cpp
    )
  endif()
else()
  message(FATAL_ERROR "Unknown OPENER_NET_BACKEND: ${OPENER_NET_BACKEND}")
endif()

if(OPENER_BUILD_NETWORK_LAYER)
  list(APPEND HAL_SRC opener_hal_adapters.c)
endif()

add_library(OPENER_HAL ${HAL_SRC})

target_include_directories(OPENER_HAL PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../config
  ${CMAKE_CURRENT_SOURCE_DIR}/../cip
  ${CMAKE_CURRENT_SOURCE_DIR}/../utils
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../ports
)

if(OPENER_NET_BACKEND STREQUAL "netburner")
  target_include_directories(OPENER_HAL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner/nndk_overload
    ${OPENER_NNDK_ROOT}/nbrtos/include
    ${OPENER_NNDK_ROOT}/arch/${OPENER_NNDK_ARCH}/include
    ${OPENER_NNDK_ROOT}/arch/${OPENER_NNDK_ARCH}/cpu/${OPENER_NNDK_CPU}/include
    ${OPENER_NNDK_ROOT}/platform/${OPENER_NNDK_PLATFORM}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports/netburner
  )
  target_compile_definitions(OPENER_HAL PRIVATE
    OPENER_NET_BACKEND_NETBURNER=1
  )
  target_compile_options(OPENER_HAL PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-URESTRICT>"
    "$<$<COMPILE_LANGUAGE:CXX>:-DRESTRICT=>"
  )
endif()

if( OPENER_INSTALL_AS_LIB )
  install(TARGETS OPENER_HAL
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()
