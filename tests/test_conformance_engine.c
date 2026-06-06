/*
 * test_conformance_engine.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/conformance/leap_conformance.h"
#include "leap/conformance/leap_conformance_capabilities.h"
#include "leap/conformance/leap_conformance_scenario.h"
#include "leap/leap_dir_controller_capabilities.h"

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

static int mock_probe_capabilities(
    void*                    user_ctx,
    const uint8_t*           peer_mac,
    LeapConformanceDeviceCaps* caps_out)
{
    LeapDirControllerCapabilities dir_caps;
    LeapEndpointDescriptor*       out_ep;
    LeapEndpointDescriptor*       in_ep;

    (void)user_ctx;
    (void)peer_mac;

    if (caps_out == NULL)
    {
        return -1;
    }

    leap_dir_controller_capabilities_init(&dir_caps);
    dir_caps.identity.product_code   = 0x0868A016u;
    dir_caps.default_profile_id      = LEAP_PROFILE_DIGITAL_IO_16X16;
    dir_caps.active_profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    dir_caps.locate_capability_flags = LEAP_LOCATE_FLAG_LED;
    dir_caps.profile.profile_id      = LEAP_PROFILE_DIGITAL_IO_16X16;
    dir_caps.profile.endpoint_count  = 2u;
    dir_caps.has_profile_descriptor  = 1;

    out_ep = &dir_caps.endpoints[0];
    out_ep->endpoint_id = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    out_ep->direction   = (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE;
    out_ep->byte_length = 2u;
    out_ep->profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;

    in_ep = &dir_caps.endpoints[1];
    in_ep->endpoint_id = LEAP_ENDPOINT_DIGITAL_INPUTS;
    in_ep->direction   = (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER;
    in_ep->byte_length = 2u;
    in_ep->profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;

    dir_caps.endpoint_count = 2u;
    leap_dir_controller_capabilities_finalize(&dir_caps);
    leap_conformance_device_caps_from_dir(&dir_caps, caps_out);
    return caps_out->valid ? 0 : -1;
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
    mock_probe_capabilities,
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

    scenario = leap_conformance_scenario_by_id("device_conformance");
    ASSERT_TRUE(scenario != NULL);
    ASSERT_TRUE(scenario->step_count >= 9u);
    ASSERT_TRUE(strcmp(scenario->id, "device_conformance") == 0);

    scenario = leap_conformance_scenario_at(0u);
    ASSERT_TRUE(scenario != NULL);

    ASSERT_TRUE(leap_conformance_scenario_count() >= 1u);
}

TEST(test_conformance_parse_profile_object)
{
    LeapDirControllerCapabilities dir_caps;
    uint8_t                       object_buf[128];
    LeapProfileDescriptor*        desc;
    LeapEndpointDescriptor*       out_ep;
    LeapEndpointDescriptor*       in_ep;
    size_t                        object_len;

    memset(object_buf, 0, sizeof(object_buf));
    desc = (LeapProfileDescriptor*)object_buf;
    desc->profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    desc->endpoint_count   = 2u;
    desc->profile_revision = 1u;

    out_ep = (LeapEndpointDescriptor*)(object_buf + sizeof(LeapProfileDescriptor));
    out_ep->endpoint_id = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    out_ep->direction   = (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE;
    out_ep->byte_length = 2u;

    in_ep = out_ep + 1;
    in_ep->endpoint_id = LEAP_ENDPOINT_DIGITAL_INPUTS;
    in_ep->direction   = (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER;
    in_ep->byte_length = 2u;

    object_len = sizeof(LeapProfileDescriptor) + (2u * sizeof(LeapEndpointDescriptor));

    leap_dir_controller_capabilities_init(&dir_caps);
    ASSERT_EQ_INT(
        leap_dir_controller_parse_profile_object(
            object_buf, object_len, &dir_caps),
        LEAP_DIR_CTRL_OK);
    ASSERT_TRUE(dir_caps.valid);
    ASSERT_TRUE(dir_caps.endpoint_count == 2u);
    ASSERT_TRUE(dir_caps.has_digital_outputs);
    ASSERT_TRUE(dir_caps.has_digital_inputs);
    ASSERT_EQ_U16(dir_caps.output_bit_count, 16u);
}

TEST(test_conformance_caps_fallback_from_zeroed_endpoints)
{
    LeapDirControllerCapabilities dir_caps;

    leap_dir_controller_capabilities_init(&dir_caps);
    dir_caps.profile.profile_id     = LEAP_PROFILE_DIGITAL_IO_16X16;
    dir_caps.active_profile_id      = LEAP_PROFILE_DIGITAL_IO_16X16;
    dir_caps.has_profile_descriptor = 1;
    dir_caps.endpoint_count         = 2u;
    /* leave endpoints[0..1] as zero (id=0, direction=0, byte_length=0) */

    leap_dir_controller_capabilities_finalize(&dir_caps);

    ASSERT_TRUE(dir_caps.valid);
    ASSERT_TRUE(dir_caps.has_digital_outputs);
    ASSERT_TRUE(dir_caps.has_digital_inputs);
    ASSERT_EQ_U16(dir_caps.output_bit_count, 16u);
    ASSERT_EQ_U16(dir_caps.input_bit_count, 16u);
    /* pd_map should have been populated via profile fallback too */
    ASSERT_TRUE(dir_caps.pd_map.valid);
}

TEST(test_conformance_caps_generate_masks)
{
    LeapDirControllerCapabilities dir_caps;
    LeapConformanceDeviceCaps     caps;

    leap_dir_controller_capabilities_init(&dir_caps);
    dir_caps.active_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    dir_caps.profile.profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    dir_caps.endpoints[0].endpoint_id = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    dir_caps.endpoints[0].direction =
        (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE;
    dir_caps.endpoints[0].byte_length = 2u;
    dir_caps.endpoint_count = 1u;
    leap_dir_controller_capabilities_finalize(&dir_caps);

    leap_conformance_device_caps_from_dir(&dir_caps, &caps);
    ASSERT_TRUE(caps.valid);
    ASSERT_TRUE(caps.pd_mask_count == 16u);
    ASSERT_TRUE(caps.pd_masks[0].mask == 0x0001u);
    ASSERT_TRUE(strcmp(caps.pd_masks[0].label, "output ch 1") == 0);

    leap_dir_controller_capabilities_init(&dir_caps);
    dir_caps.active_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    leap_dir_controller_capabilities_finalize(&dir_caps);
    leap_conformance_device_caps_from_dir(&dir_caps, &caps);
    ASSERT_TRUE(caps.dir.output_bit_count == 0u);
    ASSERT_TRUE(caps.pd_mask_count == 0u);
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
    config.scenario_id   = NULL;
    config.adapter       = "\\Device\\NPF_mock";
    config.peer_mac_text = "94:51:dc:21:f0:2f";
    config.has_peer_mac  = 1;
    memcpy(config.peer_mac, peer_mac, 6);
    config.cyclic_seconds     = 1u;
    config.cyclic_period_ms   = 100u;
    config.io                 = &g_mock_io;

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
    config.scenario_id       = NULL;
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
    RUN_TEST(test_conformance_caps_generate_masks);
    RUN_TEST(test_conformance_mock_run_pass);
    RUN_TEST(test_conformance_step_filter);
}
