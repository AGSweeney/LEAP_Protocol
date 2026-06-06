/*
 * test_harness.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_TEST_HARNESS_H
#define LEAP_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern int g_tests_run;
extern int g_tests_failed;
extern int g_current_test_failed;

#define TEST(name) static void name(void)

#define RUN_TEST(name)                                  \
    do                                                  \
    {                                                   \
        g_tests_run++;                                  \
        g_current_test_failed = 0;                      \
        printf("  %s ... ", #name);                     \
        name();                                         \
        if (g_current_test_failed != 0)                 \
        {                                               \
            g_tests_failed++;                           \
            printf("FAIL\n");                           \
        }                                               \
        else                                            \
        {                                               \
            printf("ok\n");                             \
        }                                               \
    } while (0)

#define ASSERT_TRUE(expr)                               \
    do                                                  \
    {                                                   \
        if (!(expr))                                    \
        {                                               \
            g_current_test_failed = 1;                  \
            printf("\n    %s:%d: %s\n",                 \
                   __FILE__, __LINE__, #expr);          \
            return;                                     \
        }                                               \
    } while (0)

#define ASSERT_TRUE_RET(expr, retv)                     \
    do                                                  \
    {                                                   \
        if (!(expr))                                    \
        {                                               \
            g_current_test_failed = 1;                  \
            printf("\n    %s:%d: %s\n",                 \
                   __FILE__, __LINE__, #expr);          \
            return (retv);                              \
        }                                               \
    } while (0)

#define ASSERT_EQ_U32(a, b)                             \
    do                                                  \
    {                                                   \
        const uint32_t _a = (uint32_t)(a);              \
        const uint32_t _b = (uint32_t)(b);              \
        if (_a != _b)                                   \
        {                                               \
            g_current_test_failed = 1;                  \
            printf("\n    %s:%d: expected 0x%08X got 0x%08X\n", \
                   __FILE__, __LINE__, _b, _a);         \
            return;                                     \
        }                                               \
    } while (0)

#define ASSERT_EQ_U16(a, b)                             \
    do                                                  \
    {                                                   \
        const uint16_t _a = (uint16_t)(a);              \
        const uint16_t _b = (uint16_t)(b);              \
        if (_a != _b)                                   \
        {                                               \
            g_current_test_failed = 1;                  \
            printf("\n    %s:%d: expected 0x%04X got 0x%04X\n", \
                   __FILE__, __LINE__, _b, _a);         \
            return;                                     \
        }                                               \
    } while (0)

#define ASSERT_EQ_U16_RET(a, b, retv)                   \
    do                                                  \
    {                                                   \
        const uint16_t _a = (uint16_t)(a);              \
        const uint16_t _b = (uint16_t)(b);              \
        if (_a != _b)                                   \
        {                                               \
            g_current_test_failed = 1;                  \
            printf("\n    %s:%d: expected 0x%04X got 0x%04X\n", \
                   __FILE__, __LINE__, _b, _a);         \
            return (retv);                              \
        }                                               \
    } while (0)

#define ASSERT_EQ_INT(a, b)                             \
    do                                                  \
    {                                                   \
        const int _a = (int)(a);                        \
        const int _b = (int)(b);                        \
        if (_a != _b)                                   \
        {                                               \
            g_current_test_failed = 1;                  \
            printf("\n    %s:%d: expected %d got %d\n", \
                   __FILE__, __LINE__, _b, _a);         \
            return;                                     \
        }                                               \
    } while (0)

#define ASSERT_EQ_INT_RET(a, b, retv)                   \
    do                                                  \
    {                                                   \
        const int _a = (int)(a);                        \
        const int _b = (int)(b);                        \
        if (_a != _b)                                   \
        {                                               \
            g_current_test_failed = 1;                  \
            printf("\n    %s:%d: expected %d got %d\n", \
                   __FILE__, __LINE__, _b, _a);         \
            return (retv);                              \
        }                                               \
    } while (0)

int leap_test_summary(void);

#endif /* LEAP_TEST_HARNESS_H */
