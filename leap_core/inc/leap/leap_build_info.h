/*
 * leap_build_info.h
 *
 * Build metadata banner (git hash, UTC build date).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_BUILD_INFO_H
#define LEAP_BUILD_INFO_H

#include "leap_build_info_gen.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void leap_build_info_print(FILE* stream, const char* program_name);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_BUILD_INFO_H */
