/*
 * leap_win_smoke.h
 *
 * Windows wire smoke test — options, validation, and user-facing diagnostics.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_WIN_SMOKE_H
#define LEAP_WIN_SMOKE_H

#include "leap/leap_controller_stack.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_raw_winpcap.h"

#define LEAP_WIN_SMOKE_DEFAULT_CYCLES     15u
#define LEAP_WIN_SMOKE_DEFAULT_CYCLE_MS   50u
#define LEAP_WIN_SMOKE_DEFAULT_OUTPUTS    0x0015u
#define LEAP_WIN_SMOKE_DEFAULT_LEASE_US   5000000u
#define LEAP_WIN_SMOKE_SHORT_LEASE_US     2000000u
#define LEAP_WIN_SMOKE_LEASE_IDLE_MS      3500u

typedef struct LeapWinSmokeOptions
{
    char     adapter[LEAP_RAW_WINPCAP_NAME_MAX];
    unsigned cycles;
    unsigned cycle_ms;
    uint16_t pd_outputs;
    int      list_adapters;
    int      skip_cyclic;
    int      skip_lease_test;
    int      verbose;
    int      no_color;
} LeapWinSmokeOptions;

typedef struct LeapWinSmokeReport
{
    unsigned passed;
    unsigned planned;
    int      color_enabled;
} LeapWinSmokeReport;

void leap_win_smoke_options_defaults(LeapWinSmokeOptions* options);

void leap_win_smoke_console_init(LeapWinSmokeOptions* options);

void leap_win_smoke_report_init(
    LeapWinSmokeReport*            report,
    const LeapWinSmokeOptions*     options);

void leap_win_smoke_pass(LeapWinSmokeReport* report, const char* label);

void leap_win_smoke_fail(LeapWinSmokeReport* report, const char* label);

void leap_win_smoke_print_summary(const LeapWinSmokeReport* report);

int leap_win_smoke_parse_args(
    int                      argc,
    char**                   argv,
    LeapWinSmokeOptions*     options);

void leap_win_smoke_print_usage(const char* program);

void leap_win_smoke_print_adapter_hint(void);

int leap_win_smoke_open_transport(
    LeapRawWinpcapSocket*       transport,
    const LeapWinSmokeOptions*  options);

void leap_win_smoke_print_transport_stats(const LeapRawWinpcapSocket* transport);

int leap_win_smoke_validate_bootstrap(
    const LeapControllerStack* stack,
    const LeapDeviceStack*     device,
    const uint8_t*             controller_mac);

int leap_win_smoke_validate_outputs(
    uint16_t actual,
    uint16_t expected);

int leap_win_smoke_run_cyclic(
    LeapControllerStack*        stack,
    const LeapPdControllerIo*   pd_io,
    const LeapWinSmokeOptions*  options,
    LeapWinSmokeReport*         report);

typedef void (*LeapWinSmokePumpFn)(void* user_ctx, unsigned max_frames);

int leap_win_smoke_run_lease_expiry(
    LeapControllerStack*         stack,
    const LeapControllerStackIo* stack_io,
    const LeapPdControllerIo*    pd_io,
    LeapDeviceStack*             device,
    int*                         hello_reply_gate,
    const uint8_t*               controller_mac,
    uint16_t*                    digital_outputs,
    uint16_t                     pd_outputs,
    LeapWinSmokePumpFn           pump,
    void*                        pump_ctx,
    LeapWinSmokeReport*          report);

void leap_win_smoke_reset_hello_gate(int* hello_reply_gate);

#endif /* LEAP_WIN_SMOKE_H */
