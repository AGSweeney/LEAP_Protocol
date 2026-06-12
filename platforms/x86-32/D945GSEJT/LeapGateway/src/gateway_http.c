/*
 * gateway_http.c — HTTP server for gateway Web UI + REST.
 *
 * Runs from the init task poll loop (same context as OpENer) so TCP accept
 * behaves reliably on RTEMS libbsd.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_http.h"

#include "gateway_config.h"
#include "gateway_global.h"
#include "gateway_leap_session.h"
#include "gateway_storage.h"
#include "gateway_web_index.h"
#include "leap_time.h"

#include "leap/leap_controller_session_hub.h"
#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_gateway_config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/reboot.h>
#elif defined(__rtems__)
#include <rtems.h>
#endif

#define LEAP_GATEWAY_HTTP_REQUEST_MAX 4096u
#define LEAP_GATEWAY_HTTP_BODY_MAX    4096u
#define LEAP_GATEWAY_HTTP_RECV_MS     3000
#define LEAP_GATEWAY_HTTP_LISTEN_MAX  2

static int   g_listen_fds[LEAP_GATEWAY_HTTP_LISTEN_MAX];
static int   g_listen_count;
static char  g_http_request[LEAP_GATEWAY_HTTP_REQUEST_MAX];
static char  g_http_body[LEAP_GATEWAY_HTTP_BODY_MAX];
static int   g_reboot_pending;

static int
http_mac_is_zero(const uint8_t mac[6])
{
    static const uint8_t zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };

    return memcmp(mac, zero, 6) == 0;
}

static int
http_peer_is_owned_live(const uint8_t peer_mac[6])
{
    unsigned i;

    if (peer_mac == NULL)
    {
        return 0;
    }

    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; ++i)
    {
        if (g_gateway.session_hub.slots[i].in_use == 0)
        {
            continue;
        }

        if (memcmp(g_gateway.session_hub.slots[i].peer_mac, peer_mac, 6) != 0)
        {
            continue;
        }

        return leap_controller_session_hub_is_op(&g_gateway.session_hub, (int)i);
    }

    return 0;
}

static int
http_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0)
    {
        return -1;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void
http_send_all(int fd, const char* data, size_t len)
{
    size_t sent = 0u;

    while (sent < len)
    {
        ssize_t n = send(fd, data + sent, len - sent, 0);

        if (n <= 0)
        {
            break;
        }
        sent += (size_t)n;
    }
}

static void
http_reply(int fd, int code, const char* ctype, const char* body, size_t body_len)
{
    char        header[256];
    const char* status = "OK";

    if (code == 404)
    {
        status = "Not Found";
    }
    else if (code == 400)
    {
        status = "Bad Request";
    }
    else if (code == 204)
    {
        status = "No Content";
    }
    else if (code == 503)
    {
        status = "Service Unavailable";
    }

    (void)snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n",
        code,
        status,
        ctype,
        body_len);
    http_send_all(fd, header, strlen(header));
    if (body != NULL && body_len > 0u)
    {
        http_send_all(fd, body, body_len);
    }
}

static void
http_reply_cstr(int fd, int code, const char* ctype, const char* body)
{
    size_t body_len = body != NULL ? strlen(body) : 0u;

    http_reply(fd, code, ctype, body, body_len);
}

static void
http_reboot_if_pending(void)
{
    if (g_reboot_pending == 0)
    {
        return;
    }

    printf(
        LEAP_TS_FMT LEAP_ANSI_WARN "Gateway: reboot requested from Web UI" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());
    sync();

#if defined(__linux__)
    (void)reboot(RB_AUTOBOOT);
    printf(
        LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: reboot syscall failed: %s" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str(),
        strerror(errno));
    g_reboot_pending = 0;
#elif defined(__rtems__)
    rtems_shutdown_executive(0);
#else
    printf(
        LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: reboot unsupported on this platform" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());
    g_reboot_pending = 0;
#endif
}

static int
http_set_recv_timeout(int fd, int timeout_ms)
{
    struct timeval tv;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

static int
http_read_request(int fd, char* buf, size_t capacity)
{
    size_t total = 0u;
    char*  header_end;

    buf[0] = '\0';

    while (total + 1u < capacity)
    {
        ssize_t n = recv(fd, buf + total, capacity - 1u - total, 0);

        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (n == 0)
        {
            break;
        }

        total += (size_t)n;
        buf[total] = '\0';

        header_end = strstr(buf, "\r\n\r\n");
        if (header_end != NULL)
        {
            return (int)total;
        }
    }

    return total > 0u ? (int)total : -1;
}

static int
http_parse_request_line(
    const char* request,
    char*       method_out,
    size_t      method_cap,
    char*       path_out,
    size_t      path_cap)
{
    const char* line_end;
    const char* sp1;
    const char* sp2;
    size_t      method_len;
    size_t      path_len;

    line_end = strstr(request, "\r\n");
    if (line_end == NULL)
    {
        return -1;
    }

    sp1 = strchr(request, ' ');
    if (sp1 == NULL || sp1 >= line_end)
    {
        return -1;
    }

    sp2 = strchr(sp1 + 1, ' ');
    if (sp2 == NULL || sp2 >= line_end)
    {
        return -1;
    }

    method_len = (size_t)(sp1 - request);
    path_len = (size_t)(sp2 - sp1 - 1);
    if (method_len + 1u > method_cap || path_len + 1u > path_cap)
    {
        return -1;
    }

    memcpy(method_out, request, method_len);
    method_out[method_len] = '\0';
    memcpy(path_out, sp1 + 1, path_len);
    path_out[path_len] = '\0';

    {
        char* query = strchr(path_out, '?');

        if (query != NULL)
        {
            *query = '\0';
        }
    }

    return 0;
}

static const char*
http_body(const char* request)
{
    const char* marker = strstr(request, "\r\n\r\n");

    return marker != NULL ? marker + 4 : NULL;
}

static void
append_peer_json(char* buf, size_t cap, size_t* used)
{
    unsigned i;

    (void)snprintf(buf + *used, cap - *used, "{\"peers\":[");
    *used = strlen(buf);

    for (i = 0u; i < g_gateway.peer_table.count; ++i)
    {
        const LeapControllerPeerEntry* peer =
            leap_controller_peer_table_get(&g_gateway.peer_table, i);

        if (peer == NULL)
        {
            continue;
        }

        if (http_peer_is_owned_live(peer->mac) != 0)
        {
            continue;
        }

        {
            uint16_t      state = peer->device_state;
            const uint8_t* owner = peer->active_owner_mac;

            if (*used > 12u && buf[*used - 1u] != '[')
            {
                (void)snprintf(buf + *used, cap - *used, ",");
                *used = strlen(buf);
            }

            if (http_mac_is_zero(owner))
            {
                (void)snprintf(
                    buf + *used,
                    cap - *used,
                    "{\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"profile\":\"0x%08X\",\"state\":\"0x%04X\",\"owner\":\"none\"}",
                    peer->mac[0],
                    peer->mac[1],
                    peer->mac[2],
                    peer->mac[3],
                    peer->mac[4],
                    peer->mac[5],
                    (unsigned)peer->active_profile_id,
                    (unsigned)state);
            }
            else
            {
                (void)snprintf(
                    buf + *used,
                    cap - *used,
                    "{\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"profile\":\"0x%08X\",\"state\":\"0x%04X\",\"owner\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",
                    peer->mac[0],
                    peer->mac[1],
                    peer->mac[2],
                    peer->mac[3],
                    peer->mac[4],
                    peer->mac[5],
                    (unsigned)peer->active_profile_id,
                    (unsigned)state,
                    owner[0],
                    owner[1],
                    owner[2],
                    owner[3],
                    owner[4],
                    owner[5]);
            }
        }
        *used = strlen(buf);
    }

    (void)snprintf(
        buf + *used,
        cap - *used,
        "],\"discover_active\":%s}",
        g_gateway.discover_active ? "true" : "false");
}

static const char*
leap_phase_name(LeapControllerStackPhase phase)
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

static void
gateway_http_leap_phase_text(char* buf, size_t cap)
{
    unsigned op_count;
    unsigned i;

    if (buf == NULL || cap == 0u)
    {
        return;
    }

    op_count = leap_controller_session_hub_count_op_peers(&g_gateway.session_hub);
    if (op_count == 0u)
    {
        (void)snprintf(buf, cap, "IDLE");
        return;
    }

    if (op_count == 1u)
    {
        for (i = 0u; i < LEAP_CTRL_MAX_PEERS; ++i)
        {
            if (leap_controller_session_hub_is_op(&g_gateway.session_hub, (int)i) != 0)
            {
                LeapControllerStack* stack =
                    leap_controller_session_hub_stack(&g_gateway.session_hub, (int)i);

                if (stack != NULL)
                {
                    (void)snprintf(
                        buf,
                        cap,
                        "%s",
                        leap_phase_name(leap_controller_stack_get_phase(stack)));
                    return;
                }
            }
        }
    }

    (void)snprintf(buf, cap, "OP(%u)", op_count);
}

static void
append_io_json(char* buf, size_t cap, size_t* used)
{
    unsigned i;
    char     phase_text[24];
    int      session_active;
    int      io_emitted = 0;

    gateway_http_leap_phase_text(phase_text, sizeof(phase_text));
    session_active = leap_gateway_leap_session_active(&g_gateway) ? 1 : 0;

    (void)snprintf(
        buf + *used,
        cap - *used,
        "{\"session_active\":%s,\"leap_phase\":\"%s\",\"leap_phase_code\":%u,"
        "\"leap_comm_ok\":%s,\"mappings\":[",
        session_active ? "true" : "false",
        phase_text,
        (unsigned)leap_controller_session_hub_count_op_peers(&g_gateway.session_hub),
        g_gateway.bridge.leap_comm_ok ? "true" : "false");
    *used = strlen(buf);

    for (i = 0u; i < g_gateway.config.bridge.mapping_count; ++i)
    {
        const LeapEipBridgeMapping* map = &g_gateway.config.bridge.mappings[i];

        if (!map->enabled ||
            (map->leap_mac[0] == 0u && map->leap_mac[1] == 0u &&
             map->leap_mac[2] == 0u && map->leap_mac[3] == 0u &&
             map->leap_mac[4] == 0u && map->leap_mac[5] == 0u))
        {
            continue;
        }

        const LeapEipBridgePeerIo*  peer = &g_gateway.bridge.peer_io[i];
        uint8_t                     eip_in_byte = 0u;
        uint8_t                     eip_out_byte = 0u;
        uint16_t                    leap_outputs = 0u;
        uint8_t                     packed_input[LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES];
        size_t                      packed_input_len = 0u;

        (void)leap_eip_bridge_peer_outputs(&g_gateway.bridge, i, &leap_outputs);

        if (leap_eip_bridge_pack_input_assembly(
                &g_gateway.bridge,
                packed_input,
                sizeof(packed_input),
                &packed_input_len) == 0 &&
            map->input.assembly_byte < packed_input_len)
        {
            eip_in_byte = packed_input[map->input.assembly_byte];
        }
        if (map->output.assembly_byte < g_gateway.bridge.config.output_assembly_size)
        {
            eip_out_byte =
                g_gateway.bridge.output_assembly[map->output.assembly_byte];
        }

        if (io_emitted != 0)
        {
            (void)snprintf(buf + *used, cap - *used, ",");
            *used = strlen(buf);
        }
        io_emitted = 1;

        (void)snprintf(
            buf + *used,
            cap - *used,
            "{\"index\":%u,\"enabled\":%s,"
            "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\","
            "\"leap_connected\":%s,"
            "\"comm_ok\":%s,"
            "\"leap_inputs\":%u,\"leap_outputs\":%u,"
            "\"io_status\":%u,"
            "\"eip_input_byte\":%u,\"eip_output_byte\":%u,"
            "\"input_width_bits\":%u,\"output_width_bits\":%u}",
            i,
            map->enabled ? "true" : "false",
            map->leap_mac[0],
            map->leap_mac[1],
            map->leap_mac[2],
            map->leap_mac[3],
            map->leap_mac[4],
            map->leap_mac[5],
            leap_controller_session_hub_is_op(&g_gateway.session_hub, (int)i) ?
                "true" : "false",
            peer->comm_ok ? "true" : "false",
            (unsigned)peer->digital_inputs,
            (unsigned)leap_outputs,
            (unsigned)peer->io_status,
            (unsigned)eip_in_byte,
            (unsigned)eip_out_byte,
            (unsigned)map->input.width_bits,
            (unsigned)map->output.width_bits);
        *used = strlen(buf);
    }

    (void)snprintf(buf + *used, cap - *used, "]}");
}

static void
handle_request(int client_fd, const char* request)
{
    char        method[16];
    char        path[256];
    size_t      used = 0u;
    const char* payload;

    if (http_parse_request_line(
            request,
            method,
            sizeof(method),
            path,
            sizeof(path)) != 0)
    {
        http_reply_cstr(client_fd, 400, "text/plain", "bad request line");
        return;
    }

    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0))
    {
        http_reply(
            client_fd,
            200,
            "text/html; charset=utf-8",
            leap_gateway_web_index_html,
            leap_gateway_web_index_html_len);
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v1/config") == 0)
    {
        if (leap_gateway_config_export_text(
                &g_gateway.config,
                g_http_body,
                sizeof(g_http_body)) != 0)
        {
            http_reply_cstr(client_fd, 400, "text/plain", "export failed");
            return;
        }

        http_reply_cstr(client_fd, 200, "text/plain; charset=utf-8", g_http_body);
        return;
    }

    if (strcmp(method, "PUT") == 0 && strcmp(path, "/api/v1/config") == 0)
    {
        LeapGatewayConfig staged;

        payload = http_body(request);
        if (payload == NULL)
        {
            http_reply_cstr(client_fd, 400, "text/plain", "missing body");
            return;
        }

        if (strlen(payload) >= sizeof(g_http_body))
        {
            http_reply_cstr(client_fd, 400, "text/plain", "body too large");
            return;
        }

        memcpy(g_http_body, payload, strlen(payload) + 1u);
        if (leap_gateway_config_load_text(&staged, g_http_body) != 0)
        {
            http_reply_cstr(client_fd, 400, "text/plain", "invalid config");
            return;
        }

        if (leap_gateway_runtime_apply_config(&staged) != 0)
        {
            http_reply_cstr(client_fd, 400, "text/plain", "apply failed");
            return;
        }
        leap_gateway_leap_session_request_auto_connect(&g_gateway);

        http_reply_cstr(
            client_fd,
            200,
            "application/json",
            "{\"ok\":true,\"config_applied\":true,\"connect_pending\":true}");
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v1/leap/connect") == 0)
    {
        char phase_text[24];

        leap_gateway_leap_session_request_connect(&g_gateway);
        gateway_http_leap_phase_text(phase_text, sizeof(phase_text));

        (void)snprintf(
            g_http_body,
            sizeof(g_http_body),
            "{\"ok\":true,\"connect_pending\":true,\"leap_phase\":\"%s\"}",
            phase_text);
        http_reply_cstr(client_fd, 200, "application/json", g_http_body);
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v1/config/apply") == 0)
    {
        if (!leap_gateway_storage_ready())
        {
            http_reply_cstr(
                client_fd,
                503,
                "text/plain",
                "config volume not mounted (boot from CF/IDE image, not read-only ISO)");
            return;
        }

        if (leap_gateway_runtime_persist_config() != 0)
        {
            char err[128];

            (void)snprintf(
                err,
                sizeof(err),
                "save failed: %s",
                strerror(errno));
            http_reply_cstr(client_fd, 400, "text/plain", err);
            return;
        }

        http_reply_cstr(client_fd, 200, "application/json", "{\"ok\":true,\"saved\":true}");
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v1/system/reboot") == 0)
    {
        g_reboot_pending = 1;
        http_reply_cstr(
            client_fd,
            200,
            "application/json",
            "{\"ok\":true,\"reboot_pending\":true}");
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v1/status") == 0)
    {
        char phase_text[24];

        gateway_http_leap_phase_text(phase_text, sizeof(phase_text));
        (void)snprintf(
            g_http_body,
            sizeof(g_http_body),
            "{\"product\":\"LeapOS-Gateway\",\"ifname\":\"%s\",\"ipv4\":\"%s\","
            "\"http_port\":%u,\"ui_version\":6,\"storage_ready\":%s,"
            "\"config_path\":\"%s\",\"leap_comm_ok\":%s,\"leap_phase\":\"%s\","
            "\"leap_session_active\":%s,\"leap_op_peers\":%u,\"mappings\":%u}",
            g_gateway.bound_ifname,
            g_gateway.config.network.ipv4_addr,
            (unsigned)LEAP_GATEWAY_HTTP_PORT,
            leap_gateway_storage_ready() ? "true" : "false",
            g_gateway.config.config_path,
            g_gateway.bridge.leap_comm_ok ? "true" : "false",
            phase_text,
            leap_gateway_leap_session_active(&g_gateway) ? "true" : "false",
            leap_controller_session_hub_count_op_peers(&g_gateway.session_hub),
            g_gateway.config.bridge.mapping_count);
        http_reply_cstr(client_fd, 200, "application/json", g_http_body);
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v1/leap/discover") == 0)
    {
        g_gateway.discover_pending_ms = 2500;
        g_gateway.discover_active = 1;
        http_reply_cstr(
            client_fd,
            200,
            "application/json",
            "{\"ok\":true,\"scan_ms\":2500}");
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v1/leap/peers") == 0)
    {
        append_peer_json(g_http_body, sizeof(g_http_body), &used);
        http_reply_cstr(client_fd, 200, "application/json", g_http_body);
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v1/io") == 0)
    {
        append_io_json(g_http_body, sizeof(g_http_body), &used);
        http_reply_cstr(client_fd, 200, "application/json", g_http_body);
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/favicon.ico") == 0)
    {
        http_reply(client_fd, 204, "image/x-icon", NULL, 0u);
        return;
    }

    http_reply_cstr(client_fd, 404, "text/plain", "not found");
}

static int
http_listen_socket(uint16_t port)
{
    struct sockaddr_in addr;
    int                fd;
    int                opt = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "HTTP: socket(%u) failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            (unsigned)port,
            strerror(errno));
        return -1;
    }

    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN "HTTP: bind(%u) failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            (unsigned)port,
            strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 8) != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "HTTP: listen(%u) failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            (unsigned)port,
            strerror(errno));
        close(fd);
        return -1;
    }

    if (http_set_nonblocking(fd) != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "HTTP: fcntl(%u) failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            (unsigned)port,
            strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static void
http_serve_client(int client_fd)
{
    int opt = 1;

    (void)setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    (void)http_set_recv_timeout(client_fd, LEAP_GATEWAY_HTTP_RECV_MS);

    if (http_read_request(client_fd, g_http_request, sizeof(g_http_request)) > 0)
    {
        handle_request(client_fd, g_http_request);
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
}

int
leap_gateway_http_init(void)
{
    int i;

    g_listen_count = 0;
    for (i = 0; i < LEAP_GATEWAY_HTTP_LISTEN_MAX; ++i)
    {
        g_listen_fds[i] = -1;
    }

    g_listen_fds[g_listen_count] = http_listen_socket(LEAP_GATEWAY_HTTP_PORT);
    if (g_listen_fds[g_listen_count] >= 0)
    {
        ++g_listen_count;
    }

#if LEAP_GATEWAY_HTTP_PORT != LEAP_GATEWAY_HTTP_PORT_ALT
    g_listen_fds[g_listen_count] = http_listen_socket(LEAP_GATEWAY_HTTP_PORT_ALT);
    if (g_listen_fds[g_listen_count] >= 0)
    {
        ++g_listen_count;
    }
#endif

    if (g_listen_count == 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "HTTP: no listen sockets available" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
        return -1;
    }

    printf(
        LEAP_TS_FMT LEAP_ANSI_OK
        "HTTP: Web UI http://%s:%u (also :%u)" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str(),
        g_gateway.config.network.ipv4_addr,
        (unsigned)LEAP_GATEWAY_HTTP_PORT,
        (unsigned)LEAP_GATEWAY_HTTP_PORT_ALT);
    return 0;
}

void
leap_gateway_http_poll(void)
{
    int i;

    if (g_listen_count <= 0)
    {
        return;
    }

    for (;;)
    {
        int served = 0;

        for (i = 0; i < g_listen_count; ++i)
        {
            struct sockaddr_in client_addr;
            socklen_t          client_len = sizeof(client_addr);
            int                client_fd;

            client_fd = accept(
                g_listen_fds[i],
                (struct sockaddr*)&client_addr,
                &client_len);
            if (client_fd >= 0)
            {
                http_serve_client(client_fd);
                http_reboot_if_pending();
                if (g_reboot_pending != 0)
                {
                    return;
                }
                served = 1;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                printf(
                    LEAP_TS_FMT LEAP_ANSI_WARN "HTTP: accept failed: %s" LEAP_ANSI_RESET "\n",
                    leap_rtems_uptime_str(),
                    strerror(errno));
            }
        }

        if (!served)
        {
            break;
        }
    }
}
