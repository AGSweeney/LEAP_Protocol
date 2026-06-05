cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Widgets Svg Concurrent REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

qt_add_executable(leap_studio_qt
  src/main.cpp
  src/core/ConformanceAdapter.cpp
  src/core/ConformanceWorker.cpp
  src/ui/MainWindow.cpp
  src/ui/AppIcon.cpp
  src/ui/DiscoveryFormat.cpp
  src/ui/DiagnosticsFormat.cpp
  src/ui/DiagnosticsLatencyChart.cpp
  src/ui/theme/ThemeManager.cpp
  src/ui/theme/ComboPopupItemDelegate.cpp
  ../leap_cli/conformance/leap_conformance_win_io.c
  ../leap_cli/win_smoke/leap_win_io.c
  ../leap_cli/win_l2/leap_win_common.c
  resources/studio_icons.qrc
)

target_include_directories(leap_studio_qt PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../leap_cli/conformance
  ${CMAKE_CURRENT_SOURCE_DIR}/../leap_cli/win_l2
  ${CMAKE_CURRENT_SOURCE_DIR}/../leap_cli/win_smoke
)

target_link_libraries(leap_studio_qt PRIVATE
  leap_core
  Qt6::Widgets
  Qt6::Svg
  Qt6::Concurrent
  ws2_32
)

if(MSVC)
  target_compile_options(leap_studio_qt PRIVATE /utf-8 /W4)
endif()

if(WIN32)
  set_target_properties(leap_studio_qt PROPERTIES WIN32_EXECUTABLE TRUE)
  target_sources(leap_studio_qt PRIVATE resources/leap_studio.rc)
endif()

# Bundle Qt DLLs/plugins beside the .exe after every build (windeployqt).
get_filename_component(_leap_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
find_program(LEAP_WINDEPLOYQT_EXECUTABLE windeployqt
  HINTS "${_leap_qt_bin_dir}"
  REQUIRED)

add_custom_command(TARGET leap_studio_qt POST_BUILD
  COMMAND "${LEAP_WINDEPLOYQT_EXECUTABLE}"
    $<IF:$<CONFIG:Debug>,--debug,--release>
    --compiler-runtime
    --no-translations
    "$<TARGET_FILE:leap_studio_qt>"
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:leap_studio_qt>"
  COMMENT "windeployqt: deploy Qt runtime for leap_studio_qt"
)

add_custom_target(leap_studio_qt_deploy
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/deploy.ps1"
    -QtRoot "${_leap_qt_bin_dir}/.."
  DEPENDS leap_studio_qt
  COMMENT "windeployqt: redeploy Qt runtime via deploy.ps1"
)
