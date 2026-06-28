/*
 * LEAP Gateway (NetBurner) - LeapOS-Gateway REST handlers (NNDK HTTP callbacks).
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef LEAPGATEWAY_MAIN_TU

extern "C" {
#include "gateway_config.h"
#include "gateway_global.h"
#include "gateway_leap_session.h"
#include "gateway_storage.h"
#include "leap_time.h"
#include "leap_transport.h"
#include "../opener/netburner_port/opener.h"
#include "../opener/netburner_port/nb_reboot.h"
#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_session_hub.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_eip_bridge.h"
}

#include <netinterface.h>

#define GW_HTTP_JSON_BUF 2048u

static bool MatchApiPath(HTTP_Request &req, const char *path)
{
    if (req.req != tGet || !req.pURL)
    {
        return false;
    }
    const char *url = req.pURL;
    while (*url == '/')
    {
        ++url;
    }
    const size_t n = strlen(path);
    if (strncmp(url, path, n) != 0)
    {
        return false;
    }
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static int http_mac_is_zero(const uint8_t mac[6])
{
    static const uint8_t zero[6] = {0};
    return memcmp(mac, zero, 6) == 0;
}

static const char *leap_phase_name(LeapControllerStackPhase phase)
{
    switch (phase)
    {
    case LEAP_CTRL_STACK_IDLE:
        return "IDLE";
    case LEAP_CTRL_STACK_DISCOVERING:
        return "DISCOVERING";
    case LEAP_CTRL_STACK_SELECT_PROFILE:
        return "SELECT_PROFILE";
    case LEAP_CTRL_STACK_OPEN_SESSION:
        return "OPEN_SESSION";
    case LEAP_CTRL_STACK_SET_STATE:
        return "SET_STATE";
    case LEAP_CTRL_STACK_OP:
        return "OP";
    case LEAP_CTRL_STACK_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static void leap_phase_text(char *buf, size_t cap)
{
    unsigned op_count = leap_controller_session_hub_count_op_peers(&g_gateway.session_hub);

    if (buf == nullptr || cap == 0u)
    {
        return;
    }

    if (op_count == 0u)
    {
        snprintf(buf, cap, "IDLE");
        return;
    }

    if (op_count == 1u)
    {
        for (unsigned i = 0u; i < LEAP_CTRL_MAX_PEERS; ++i)
        {
            if (leap_controller_session_hub_is_op(&g_gateway.session_hub, static_cast<int>(i)) != 0)
            {
                LeapControllerStack *stack =
                    leap_controller_session_hub_stack(&g_gateway.session_hub, static_cast<int>(i));

                if (stack != nullptr)
                {
                    snprintf(buf, cap, "%s", leap_phase_name(leap_controller_stack_get_phase(stack)));
                    return;
                }
            }
        }
    }

    snprintf(buf, cap, "OP(%u)", op_count);
}

static int HandleLeapStatusApi(int sock, HTTP_Request &req)
{
    (void)req;
    char phase[24]{0};
    char config_path[128]{0};
    LeapRtemsTransportStats transport_stats{};
    bool leap_comm_ok = false;
    bool discover_active = false;
    bool connect_suppressed = false;
    int discover_pending_ms = 0;
    unsigned mapping_count = 0u;

    leap_phase_text(phase, sizeof(phase));
    leap_rtems_transport_get_stats(&transport_stats);
    const int plant_if = GetFirstInterface();
    leap_gateway_runtime_lock();
    snprintf(config_path, sizeof(config_path), "%s", g_gateway.config.config_path);
    leap_comm_ok = (g_gateway.bridge.leap_comm_ok != 0);
    discover_active = (g_gateway.discover_active != 0);
    discover_pending_ms = g_gateway.discover_pending_ms;
    connect_suppressed = (g_gateway.leap_session.connect_suppressed != 0);
    mapping_count = g_gateway.config.bridge.mapping_count;
    leap_gateway_runtime_unlock();
    const bool leap_can_scan =
        (leap_gateway_leap_session_active(&g_gateway) == 0 &&
         !discover_active &&
         discover_pending_ms <= 0);

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"product\":\"LEAP-Gateway-Embedded\",\"ifname\":\"%s\",\"ipv4\":\"%hI\","
             "\"http_port\":%u,\"ui_version\":7,\"storage_ready\":%s,\"config_path\":\"%s\","
             "\"leap_comm_ok\":%s,\"leap_phase\":\"%s\","
             "\"leap_session_active\":%s,\"leap_op_peers\":%u,\"mappings\":%u,"
             "\"discover_active\":%s,\"leap_can_scan\":%s,\"connect_suppressed\":%s,\"eip_ok\":%s,"
             "\"leap_tx_frames\":%u,\"leap_tx_failures\":%u,"
             "\"leap_rx_callbacks\":%u,\"leap_rx_frames\":%u,\"leap_rx_nonmatches\":%u,"
             "\"leap_rx_wrong_interface\":%u,\"leap_rx_drops\":%u,"
             "\"leap_bound_interface\":%d,\"leap_last_rx_interface\":%d,"
             "\"leap_last_wrong_interface\":%d,\"leap_last_ethertype\":\"0x%04X\","
             "\"leap_last_nonmatch_ethertype\":\"0x%04X\"}",
             g_gateway.bound_ifname,
             InterfaceIP(plant_if),
             static_cast<unsigned>(LEAP_GATEWAY_HTTP_PORT),
             leap_gateway_storage_ready() ? "true" : "false",
             config_path,
             leap_comm_ok ? "true" : "false",
             phase,
             leap_gateway_leap_session_active(&g_gateway) ? "true" : "false",
             leap_controller_session_hub_count_op_peers(&g_gateway.session_hub),
             mapping_count,
             discover_active ? "true" : "false",
             leap_can_scan ? "true" : "false",
             connect_suppressed ? "true" : "false",
             (opener_get_status() == 0) ? "true" : "false",
             static_cast<unsigned>(transport_stats.tx_frames),
             static_cast<unsigned>(transport_stats.tx_failures),
             static_cast<unsigned>(transport_stats.rx_callbacks),
             static_cast<unsigned>(transport_stats.rx_matches),
             static_cast<unsigned>(transport_stats.rx_nonmatches),
             static_cast<unsigned>(transport_stats.rx_wrong_interface),
             static_cast<unsigned>(transport_stats.rx_drops),
             static_cast<int>(transport_stats.bound_interface),
             static_cast<int>(transport_stats.last_rx_interface),
             static_cast<int>(transport_stats.last_wrong_interface),
             static_cast<unsigned>(transport_stats.last_rx_ethertype),
             static_cast<unsigned>(transport_stats.last_nonmatch_ethertype));
    return 1;
}

static int HandleLeapConnectApi(int sock, HTTP_Request &req)
{
    (void)req;
    char phase[24]{0};

    leap_gateway_leap_session_request_connect(&g_gateway);
    leap_phase_text(phase, sizeof(phase));
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"connect_pending\":true,\"leap_phase\":\"%s\"}", phase);
    return 1;
}

static int HandleLeapDisconnectApi(int sock, HTTP_Request &req)
{
    (void)req;
    leap_gateway_leap_session_request_disconnect(&g_gateway);
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"disconnected\":true,\"connect_suppressed\":true}");
    return 1;
}

static int HandleLeapDiscoverApi(int sock, HTTP_Request &req)
{
    (void)req;
    if (leap_gateway_leap_session_active(&g_gateway))
    {
        fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock,
                 "{\"ok\":true,\"skipped\":true,\"reason\":\"scan skipped while LEAP owner sessions are active\"}");
        return 1;
    }
    g_gateway.discover_pending_ms = LEAP_GATEWAY_DISCOVER_SCAN_MS;
    g_gateway.discover_active = 1;
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"scan_ms\":%d}", LEAP_GATEWAY_DISCOVER_SCAN_MS);
    return 1;
}

static int HandleLeapPeersApi(int sock, HTTP_Request &req)
{
    (void)req;
    LeapControllerPeerTable peers;
    bool discover_active = false;

    leap_gateway_runtime_lock();
    peers = g_gateway.peer_table;
    discover_active = (g_gateway.discover_active != 0);
    leap_gateway_runtime_unlock();

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"peers\":[");
    int emitted = 0;
    for (unsigned i = 0u; i < peers.count; ++i)
    {
        const LeapControllerPeerEntry *peer = leap_controller_peer_table_get(&peers, i);
        uint8_t owner_mac[6] = {0};
        uint16_t device_state = 0u;
        int owned_by_gateway = 0;

        if (peer == nullptr)
        {
            continue;
        }
        device_state = peer->device_state;
        leap_gateway_runtime_lock();
        const int slot = leap_controller_session_hub_find(&g_gateway.session_hub, peer->mac);
        if (slot >= 0 && leap_controller_session_hub_is_op(&g_gateway.session_hub, slot))
        {
            memcpy(owner_mac, g_gateway.session_hub.config.default_peer.mgmt.controller_mac, sizeof(owner_mac));
            device_state = static_cast<uint16_t>(LEAP_STATE_OP);
            owned_by_gateway = 1;
        }
        else
        {
            memcpy(owner_mac, peer->active_owner_mac, sizeof(owner_mac));
        }
        leap_gateway_runtime_unlock();

        if (emitted != 0)
        {
            fdprintf(sock, ",");
        }
        emitted = 1;
        if (http_mac_is_zero(owner_mac))
        {
            fdprintf(sock,
                     "{\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"profile\":\"0x%08X\",\"state\":\"0x%04X\",\"owner\":\"none\"}",
                     peer->mac[0], peer->mac[1], peer->mac[2], peer->mac[3], peer->mac[4], peer->mac[5],
                     peer->active_profile_id,
                     device_state);
        }
        else
        {
            fdprintf(sock,
                     "{\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"profile\":\"0x%08X\",\"state\":\"0x%04X\",\"owner\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"owned_by_gateway\":%s}",
                     peer->mac[0], peer->mac[1], peer->mac[2], peer->mac[3], peer->mac[4], peer->mac[5],
                     peer->active_profile_id,
                     device_state,
                     owner_mac[0], owner_mac[1], owner_mac[2],
                     owner_mac[3], owner_mac[4], owner_mac[5],
                     owned_by_gateway ? "true" : "false");
        }
    }
    fdprintf(sock, "],\"discover_active\":%s}", discover_active ? "true" : "false");
    return 1;
}

static int HandleLeapIoApi(int sock, HTTP_Request &req)
{
    (void)req;
    char phase[24]{0};
    bool leap_comm_ok = false;
    unsigned mapping_count = 0u;
    leap_phase_text(phase, sizeof(phase));
    leap_gateway_runtime_lock();
    leap_comm_ok = (g_gateway.bridge.leap_comm_ok != 0);
    mapping_count = g_gateway.config.bridge.mapping_count;
    leap_gateway_runtime_unlock();

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"session_active\":%s,\"leap_phase\":\"%s\",\"leap_comm_ok\":%s,\"mappings\":[",
             leap_gateway_leap_session_active(&g_gateway) ? "true" : "false",
             phase,
             leap_comm_ok ? "true" : "false");

    int io_emitted = 0;
    for (unsigned i = 0u; i < mapping_count; ++i)
    {
        LeapEipBridgeMapping map;
        LeapEipBridgePeerIo peer;
        uint16_t leap_outputs = 0u;
        uint8_t eip_in_value = 0u;
        uint8_t eip_out_value = 0u;
        uint8_t eip_status_value = 0u;
        uint8_t packed_input[LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES];
        size_t packed_len = 0u;
        uint16_t output_assembly_size = 0u;

        leap_gateway_runtime_lock();
        map = g_gateway.config.bridge.mappings[i];
        peer = g_gateway.bridge.peer_io[i];
        output_assembly_size = g_gateway.bridge.config.output_assembly_size;

        if (!map.enabled || http_mac_is_zero(map.leap_mac))
        {
            leap_gateway_runtime_unlock();
            continue;
        }

        (void)leap_eip_bridge_peer_outputs(&g_gateway.bridge, i, &leap_outputs);
        if (leap_eip_bridge_pack_input_assembly(
                &g_gateway.bridge, packed_input, sizeof(packed_input), &packed_len) == 0 &&
            map.input.assembly_byte < packed_len)
        {
            eip_in_value = packed_input[map.input.assembly_byte];
        }
        if (map.status_assembly_byte < packed_len)
        {
            eip_status_value = packed_input[map.status_assembly_byte];
        }
        if (map.output.assembly_byte < output_assembly_size)
        {
            eip_out_value = g_gateway.bridge.output_assembly[map.output.assembly_byte];
        }
        leap_gateway_runtime_unlock();

        if (io_emitted != 0)
        {
            fdprintf(sock, ",");
        }
        io_emitted = 1;
        fdprintf(sock,
                 "{\"index\":%u,\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\","
                 "\"leap_connected\":%s,\"comm_ok\":%s,"
                 "\"leap_inputs\":%u,\"leap_outputs\":%u,\"io_status\":%u,"
                 "\"eip_input_byte\":%u,\"eip_output_byte\":%u,\"eip_status_byte\":%u,"
                 "\"eip_input_value\":%u,\"eip_output_value\":%u,\"eip_status_value\":%u}",
                 i,
                 map.leap_mac[0], map.leap_mac[1], map.leap_mac[2],
                 map.leap_mac[3], map.leap_mac[4], map.leap_mac[5],
                 leap_controller_session_hub_is_op(&g_gateway.session_hub, static_cast<int>(i)) ? "true" : "false",
                 peer.comm_ok ? "true" : "false",
                 peer.digital_inputs, leap_outputs, peer.io_status,
                 map.input.assembly_byte, map.output.assembly_byte, map.status_assembly_byte,
                 eip_in_value, eip_out_value, eip_status_value);
    }
    fdprintf(sock, "]}");
    return 1;
}

static int HandleSystemRebootApi(int sock, HTTP_Request &req)
{
    (void)req;
    nb_schedule_reboot();
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"reboot_pending\":true}");
    return 1;
}

static int HandleConfigPersistApi(int sock, HTTP_Request &req)
{
    (void)req;
    if (!leap_gateway_storage_ready())
    {
        fdprintf(sock, "HTTP/1.0 503 Service Unavailable\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"storage_not_ready\"}");
        return 1;
    }
    if (leap_gateway_storage_save_config(&g_gateway.config) != 0)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"save_failed\"}");
        return 1;
    }
    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock, "{\"ok\":true,\"saved\":true}");
    return 1;
}

static bool MatchLeapStatusApi(HTTP_Request &req) { return MatchApiPath(req, "api/leap/status"); }
static bool MatchLeapStatusV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/status"); }
static bool MatchLeapConnectApi(HTTP_Request &req) { return MatchApiPath(req, "api/leap/connect"); }
static bool MatchLeapConnectV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/leap/connect"); }
static bool MatchLeapDisconnectApi(HTTP_Request &req) { return MatchApiPath(req, "api/leap/disconnect"); }
static bool MatchLeapDisconnectV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/leap/disconnect"); }
static bool MatchLeapDiscoverApi(HTTP_Request &req) { return MatchApiPath(req, "api/leap/discover"); }
static bool MatchLeapDiscoverV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/leap/discover"); }
static bool MatchLeapPeersApi(HTTP_Request &req) { return MatchApiPath(req, "api/leap/peers"); }
static bool MatchLeapPeersV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/leap/peers"); }
static bool MatchLeapIoApi(HTTP_Request &req) { return MatchApiPath(req, "api/leap/io"); }
static bool MatchLeapIoV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/io"); }
static bool MatchConfigPersistApi(HTTP_Request &req) { return MatchApiPath(req, "api/config/persist"); }
static bool MatchConfigPersistV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/config/apply"); }
static bool MatchSystemRebootApi(HTTP_Request &req) { return MatchApiPath(req, "api/system/reboot"); }
static bool MatchSystemRebootV1Api(HTTP_Request &req) { return MatchApiPath(req, "api/v1/system/reboot"); }

#endif // LEAPGATEWAY_MAIN_TU
