function(leap_configure_build_info OUT_DIR)
    set(_leap_root "${CMAKE_SOURCE_DIR}")
    if(ARGC GREATER 1)
        set(_leap_root "${ARGV1}")
    endif()

    set(LEAP_BUILD_GIT "unknown")

    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${_leap_root}/.git")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${_leap_root}
            OUTPUT_VARIABLE _leap_git_hash
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_leap_git_hash)
            set(LEAP_BUILD_GIT "${_leap_git_hash}")
        endif()
    endif()

    string(TIMESTAMP LEAP_BUILD_DATE UTC)
    file(MAKE_DIRECTORY "${OUT_DIR}")
    configure_file(
        "${_leap_root}/leap_core/inc/leap/leap_build_info.h.in"
        "${OUT_DIR}/leap_build_info_gen.h"
        @ONLY)
endfunction()
