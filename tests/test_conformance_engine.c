/*
 * test_conformance_engine.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/conformance/leap_conformance.h"
#include "leap/conformance/leap_conformance_scenario.h"

#include <string.h>

typedef struct MockConformanceCtx
{
    unsigned discover_peers;
    int      peer_found;
    int      bootstrap_op;
    int      pd_sent;
    int      diag_ok;
    int      lease_ok;
    int      cyclic_ok;
    int      identify_ok;
    int      locate_ok;
    int      open_count;
} MockConformanceCtx;

static int mock_open(void* user_ctx, const char* adapter, const char* capture_pcap)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)adapter;
    (void)capture_pcap;

    if (ctx == NULL)
    {
        return -1;
    }

    ctx->open_count++;
    return 0;
}

static void mock_close(void* user_ctx)
{
    (void)user_ctx;
}

static int mock_discover(void* user_ctx, int scan_ms, unsigned* peer_count_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)scan_ms;

    if (ctx == NULL || peer_count_out == NULL)
    {
        return -1;
    }

    *peer_count_out = ctx->discover_peers;
    return 0;
}

static int mock_find_peer(void* user_ctx, const uint8_t* expected_mac, int* found_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)expected_mac;

    if (ctx == NULL || found_out == NULL)
    {
        return -1;
    }

    *found_out = ctx->peer_found;
    return 0;
}

static int mock_bootstrap(void* user_ctx, uint16_t outputs, int* op_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)outputs;

    if (ctx == NULL || op_out == NULL)
    {
        return -1;
    }

    *op_out = ctx->bootstrap_op;
    return 0;
}

static int mock_pd_write(void* user_ctx, uint16_t outputs, int* sent_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)outputs;

    if (ctx == NULL || sent_out == NULL)
    {
        return -1;
    }

    *sent_out = ctx->pd_sent;
    return 0;
}

static int mock_read_diag(void* user_ctx, int* ok_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    *ok_out = ctx->diag_ok;
    return 0;
}

static int mock_lease_demo(void* user_ctx, int* ok_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    *ok_out = ctx->lease_ok;
    return 0;
}

static int mock_cyclic(
    void*                  user_ctx,
    uint16_t               outputs,
    int                    exchange,
    unsigned               seconds,
    unsigned               cyclic_ms,
    LeapPdControllerStats* stats_out,
    int*                   ok_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)outputs;
    (void)exchange;
    (void)seconds;
    (void)cyclic_ms;

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    if (stats_out != NULL)
    {
        memset(stats_out, 0, sizeof(*stats_out));
        stats_out->heartbeats_sent = 2u;
    }

    *ok_out = ctx->cyclic_ok;
    return 0;
}

static int mock_identify(void* user_ctx, const uint8_t* peer_mac, int* ok_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)peer_mac;

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    *ok_out = ctx->identify_ok;
    return 0;
}

static int mock_locate(
    void*          user_ctx,
    const uint8_t* peer_mac,
    unsigned       duration_ms,
    int*           ok_out)
{
    MockConformanceCtx* ctx = (MockConformanceCtx*)user_ctx;

    (void)peer_mac;
    (void)duration_ms;

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    *ok_out = ctx->locate_ok;
    return 0;
}

static int mock_snapshot(void* user_ctx, LeapConformanceMetrics* out)
{
    (void)user_ctx;

    if (out == NULL)
    {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    return 0;
}

static void mock_cancel(void* user_ctx)
{
    (void)user_ctx;
}

static LeapConformanceIo g_mock_io = {
    NULL,
    mock_open,
    mock_close,
    mock_discover,
    mock_find_peer,
    mock_bootstrap,
    mock_pd_write,
    mock_read_diag,
    mock_lease_demo,
    mock_cyclic,
    mock_identify,
    mock_locate,
    mock_snapshot,
    mock_cancel,
};

TEST(test_conformance_scenario_lookup)
{
    const LeapConformanceScenario* scenario;

    scenario = leap_conformance_scenario_by_id("glc618wl_bench_v1");
    ASSERT_TRUE(scenario != NULL);
    ASSERT_TRUE(scenario->step_count >= 8u);
    ASSERT_TRUE(strcmp(scenario->id, "glc618wl_bench_v1") == 0);
}

TEST(test_conformance_mock_run_pass)
{
    MockConformanceCtx       mock;
    LeapConformanceRunConfig config;
    LeapConformanceRunResult result;
    LeapConformanceStatus    status;
    static const uint8_t     peer_mac[6] = { 0x94, 0x51, 0xdc, 0x21, 0xf0, 0x2f };

    memset(&mock, 0, sizeof(mock));
    mock.discover_peers = 1u;
    mock.peer_found     = 1;
    mock.bootstrap_op   = 1;
    mock.pd_sent        = 1;
    mock.diag_ok        = 1;
    mock.lease_ok       = 1;
    mock.cyclic_ok      = 1;
    mock.identify_ok    = 1;
    mock.locate_ok      = 1;

    g_mock_io.user_ctx = &mock;

    memset(&config, 0, sizeof(config));
    config.scenario_id   = "glc618wl_bench_v1";
    config.adapter       = "\\Device\\NPF_mock";
    config.peer_mac_text = "94:51:dc:21:f0:2f";
    config.has_peer_mac  = 1;
    memcpy(config.peer_mac, peer_mac, 6);
    config.cyclic_seconds = 1u;
    config.io            = &g_mock_io;

    memset(&result, 0, sizeof(result));
    status = leap_conformance_run(&config, &result);
    ASSERT_TRUE(status == LEAP_CONF_OK);
    ASSERT_TRUE(result.summary.failed == 0u);
    ASSERT_TRUE(result.summary.passed > 0u);
    ASSERT_TRUE(mock.open_count == 1);
}

TEST(test_conformance_step_filter)
{
    MockConformanceCtx       mock;
    LeapConformanceRunConfig config;
    LeapConformanceRunResult result;
    const char*              only_discover[] = { "discover" };

    memset(&mock, 0, sizeof(mock));
    mock.discover_peers = 1u;
    mock.peer_found     = 1;
    g_mock_io.user_ctx  = &mock;

    memset(&config, 0, sizeof(config));
    config.scenario_id       = "glc618wl_bench_v1";
    config.adapter           = "\\Device\\NPF_mock";
    config.peer_mac_text     = "94:51:dc:21:f0:2f";
    config.has_peer_mac      = 1;
    config.step_filter       = only_discover;
    config.step_filter_count = 1u;
    config.io                = &g_mock_io;

    memset(&result, 0, sizeof(result));
    (void)leap_conformance_run(&config, &result);
    ASSERT_TRUE(result.summary.skipped > 0u);
}

void leap_run_conformance_engine_tests(void)
{
    printf("conformance_engine:\n");
    RUN_TEST(test_conformance_scenario_lookup);
    RUN_TEST(test_conformance_mock_run_pass);
    RUN_TEST(test_conformance_step_filter);
}
