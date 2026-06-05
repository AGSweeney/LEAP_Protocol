/*
 * leap_build_info.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_build_info.h"
#include "leap/leap_protocol.h"

#include <stdio.h>

void leap_build_info_print(FILE* stream, const char* program_name)
{
    if (stream == NULL)
    {
        stream = stdout;
    }

    if (program_name == NULL || program_name[0] == '\0')
    {
        program_name = "leap";
    }

    (void)fprintf(
        stream,
        "LEAP %s protocol %u.%u git=%s built=%s\n",
        program_name,
        (unsigned)LEAP_VERSION_MAJOR,
        (unsigned)LEAP_VERSION_MINOR,
        LEAP_BUILD_GIT,
        LEAP_BUILD_DATE);
}
