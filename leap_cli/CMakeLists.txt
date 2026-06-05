if(WIN32 AND LEAP_BUILD_WIN_SMOKE)
  add_executable(leap_win_smoke
    win_smoke/wire_smoke_main.c
    win_smoke/leap_win_io.c
    win_smoke/leap_win_smoke.c
  )
  target_include_directories(leap_win_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/win_smoke
  )
  target_link_libraries(leap_win_smoke PRIVATE leap_core ws2_32)
  if(MSVC)
    target_compile_options(leap_win_smoke PRIVATE /utf-8)
  endif()
endif()

if(WIN32 AND LEAP_BUILD_WIN_L2)
  add_executable(leap_win_controller
    win_l2/controller_main.c
    win_l2/leap_win_common.c
    win_smoke/leap_win_io.c
  )
  add_executable(leap_win_device
    win_l2/device_main.c
    win_l2/leap_win_common.c
    win_l2/leap_win_device_io.c
    win_smoke/leap_win_io.c
  )
  add_executable(leap_win_discover
    win_l2/discover_main.c
    win_l2/leap_win_common.c
    win_smoke/leap_win_io.c
  )
  add_executable(leap_win_identify
    win_l2/identify_main.c
    win_l2/leap_win_common.c
    win_smoke/leap_win_io.c
  )
  add_executable(leap_win_hub
    win_l2/hub_main.c
    win_l2/leap_win_common.c
    win_smoke/leap_win_io.c
  )
  add_executable(leap_win_raw_tx
    win_l2/raw_tx_main.c
  )
  foreach(_t leap_win_controller leap_win_device leap_win_discover
           leap_win_identify leap_win_hub)
    target_include_directories(${_t} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/win_l2
      ${CMAKE_CURRENT_SOURCE_DIR}/win_smoke
    )
    target_link_libraries(${_t} PRIVATE leap_core ws2_32)
    if(MSVC)
      target_compile_options(${_t} PRIVATE /utf-8)
    endif()
  endforeach()
  target_include_directories(leap_win_raw_tx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/win_l2
  )
  target_link_libraries(leap_win_raw_tx PRIVATE leap_core ws2_32)
  if(MSVC)
    target_compile_options(leap_win_raw_tx PRIVATE /utf-8)
  endif()
endif()

if(WIN32 AND LEAP_BUILD_WIN_L2)
  add_executable(leap_conformance
    conformance/leap_conformance_main.c
    conformance/leap_conformance_win_io.c
    win_smoke/leap_win_io.c
    win_l2/leap_win_common.c
  )
  target_include_directories(leap_conformance PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/conformance
    ${CMAKE_CURRENT_SOURCE_DIR}/win_l2
    ${CMAKE_CURRENT_SOURCE_DIR}/win_smoke
  )
  target_link_libraries(leap_conformance PRIVATE leap_core ws2_32)
  if(MSVC)
    target_compile_options(leap_conformance PRIVATE /utf-8)
  endif()
endif()
