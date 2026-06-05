# Device-side LEAP protocol stack (no controller / transport layers).
set(LEAP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(LEAP_CORE "${LEAP_ROOT}/leap_core")

include(${LEAP_CORE}/cmake/LeapBuildInfo.cmake)
set(LEAP_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/leap_gen")
leap_configure_build_info("${LEAP_GEN_DIR}" "${LEAP_ROOT}")

set(LEAP_DEVICE_SRCS
    "${LEAP_CORE}/src/crc/leap_crc.c"
    "${LEAP_CORE}/src/frame/leap_frame.c"
    "${LEAP_CORE}/src/services/disc/leap_disc_device.c"
    "${LEAP_CORE}/src/services/dir/leap_dir_device.c"
    "${LEAP_CORE}/src/services/mgmt/leap_mgmt_device.c"
    "${LEAP_CORE}/src/services/mgmt/leap_mgmt_process.c"
    "${LEAP_CORE}/src/services/pd/leap_pd_device.c"
    "${LEAP_CORE}/src/services/pd/leap_pd_common.c"
    "${LEAP_CORE}/src/services/diag/leap_diag_device.c"
    "${LEAP_CORE}/src/leap_device_stack.c"
    "${LEAP_CORE}/src/leap_log.c"
    "${LEAP_CORE}/src/leap_build_info.c"
)

idf_component_register(
    SRCS ${LEAP_DEVICE_SRCS}
    INCLUDE_DIRS "${LEAP_CORE}/inc" "${LEAP_GEN_DIR}"
)
