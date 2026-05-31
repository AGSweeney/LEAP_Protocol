/*
 * test_util.h
 *
 * Minimal helpers for LEAP conformance tests.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_TEST_UTIL_H
#define LEAP_TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>

int leap_test_hex_decode(const char* hex, uint8_t* out, size_t out_capacity, size_t* out_length);

#endif /* LEAP_TEST_UTIL_H */
