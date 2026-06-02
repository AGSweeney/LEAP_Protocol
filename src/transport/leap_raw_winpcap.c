/*
 * leap_raw_winpcap.c
 *
 * Windows Npcap transport — loads wpcap.dll at runtime (no SDK required to build).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#ifndef GAA_FLAG_INCLUDE_ALL_INTERFACES
#define GAA_FLAG_INCLUDE_ALL_INTERFACES 0x0100u
#endif

#define LEAP_WINPCAP_GAA_FLAGS \
    (GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | \
     GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_ALL_INTERFACES)

#include "leap/leap_raw_winpcap.h"
#include "leap/leap_win_time.h"

#if defined(_WIN32)

#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

typedef unsigned int  bpf_u_int32;
typedef unsigned char u_char;

#define PCAP_ERRBUF_SIZE 256
#define PCAP_NETMASK_UNKNOWN 0xffffffffu

typedef struct pcap          pcap_t;
typedef struct pcap_if       pcap_if_t;
typedef struct pcap_addr     pcap_addr_t;
typedef struct bpf_program
{
    unsigned int bf_len;
    void*        bf_insns;
} bpf_program;

struct pcap_if
{
    char*        name;
    char*        description;
    pcap_addr_t* addresses;
    bpf_u_int32  flags;
    pcap_if_t*   next;
};

struct pcap_addr
{
    struct sockaddr* addr;
    struct sockaddr* netmask;
    struct sockaddr* broadaddr;
    struct sockaddr* dstaddr;
    pcap_addr_t*     next;
};

struct pcap_pkthdr
{
    struct timeval ts;
    bpf_u_int32    caplen;
    bpf_u_int32    len;
};

typedef pcap_t* (*leap_pcap_open_live_fn)(
    const char*, int, int, int, char*);
typedef void (*leap_pcap_close_fn)(pcap_t*);
typedef int (*leap_pcap_sendpacket_fn)(pcap_t*, const u_char*, int);
typedef int (*leap_pcap_next_ex_fn)(
    pcap_t*, struct pcap_pkthdr**, const u_char**);
typedef int (*leap_pcap_compile_fn)(
    pcap_t*, bpf_program*, const char*, int, bpf_u_int32);
typedef int (*leap_pcap_setfilter_fn)(pcap_t*, bpf_program*);
typedef void (*leap_pcap_freecode_fn)(bpf_program*);
typedef int (*leap_pcap_findalldevs_fn)(pcap_if_t**, char*);
typedef void (*leap_pcap_freealldevs_fn)(pcap_if_t*);
typedef int (*leap_pcap_setmintocopy_fn)(pcap_t*, int);
typedef int (*leap_pcap_set_immediate_mode_fn)(pcap_t*, int);
typedef int (*leap_pcap_setbuff_fn)(pcap_t*, int);
typedef int (*leap_pcap_setnonblock_fn)(pcap_t*, int, char*);
typedef int (*leap_pcap_setdirection_fn)(pcap_t*, int);
typedef int (*leap_pcap_set_tstamp_type_fn)(pcap_t*, int);

/* libpcap PCAP_TSTAMP_* values (stable across Npcap releases). */
#define LEAP_PCAP_TSTAMP_HOST_HIPREC 2
#define LEAP_PCAP_TSTAMP_ADAPTER     3

#define LEAP_PCAP_D_INOUT 0
#define LEAP_PCAP_D_IN    1
#define LEAP_PCAP_D_OUT   2

typedef struct LeapWinpcapApi
{
    HMODULE                   module;
    leap_pcap_open_live_fn    open_live;
    leap_pcap_close_fn        close;
    leap_pcap_sendpacket_fn   sendpacket;
    leap_pcap_next_ex_fn      next_ex;
    leap_pcap_compile_fn      compile;
    leap_pcap_setfilter_fn    setfilter;
    leap_pcap_freecode_fn     freecode;
    leap_pcap_findalldevs_fn  findalldevs;
    leap_pcap_freealldevs_fn  freealldevs;
    leap_pcap_setmintocopy_fn      setmintocopy;
    leap_pcap_set_immediate_mode_fn set_immediate_mode;
    leap_pcap_setbuff_fn           setbuff;
    leap_pcap_setnonblock_fn       setnonblock;
    leap_pcap_setdirection_fn      setdirection;
    leap_pcap_set_tstamp_type_fn   set_tstamp_type;
} LeapWinpcapApi;

static LeapWinpcapApi g_winpcap;
static int            g_winpcap_last_errno = 0;
static char           g_winpcap_last_errbuf[PCAP_ERRBUF_SIZE];

#define NPF_DISABLE_LOOPBACK 0
#define NPF_ENABLE_LOOPBACK  2

typedef void* leap_packet_adapter;
typedef leap_packet_adapter (*leap_packet_open_adapter_fn)(char*);
typedef int (*leap_packet_set_loopback_behavior_fn)(
    leap_packet_adapter,
    int);
typedef void (*leap_packet_close_adapter_fn)(leap_packet_adapter);

static HMODULE                            g_packet_module;
static leap_packet_open_adapter_fn        g_packet_open_adapter;
static leap_packet_set_loopback_behavior_fn g_packet_set_loopback;
static leap_packet_close_adapter_fn       g_packet_close_adapter;

static const uint8_t k_npcap_loopback_mac[6] = { 0x02, 0x00, 0x4C, 0x4F, 0x4F, 0x50 };

static void leap_winpcap_set_errno(int value)
{
    g_winpcap_last_errno = value;
}

static void leap_winpcap_clear_errno(void)
{
    g_winpcap_last_errno = 0;
}

int leap_raw_winpcap_last_errno(void)
{
    return g_winpcap_last_errno;
}

const char* leap_raw_winpcap_last_error(void)
{
    return g_winpcap_last_errbuf;
}

static int leap_winpcap_try_load(const char* path)
{
    if (path == NULL || path[0] == '\0')
    {
        return -1;
    }

    g_winpcap.module = LoadLibraryA(path);
    return (g_winpcap.module != NULL) ? 0 : -1;
}

static void leap_winpcap_load_packet_dll(void)
{
    char sysdir[MAX_PATH];
    char dll_path[MAX_PATH + 32];

    if (g_packet_module != NULL)
    {
        return;
    }

    if (GetSystemDirectoryA(sysdir, MAX_PATH) == 0)
    {
        return;
    }

    (void)snprintf(
        dll_path,
        sizeof(dll_path),
        "%s\\Npcap\\Packet.dll",
        sysdir);
    g_packet_module = LoadLibraryA(dll_path);
    if (g_packet_module == NULL)
    {
        return;
    }

    g_packet_open_adapter = (leap_packet_open_adapter_fn)GetProcAddress(
        g_packet_module,
        "PacketOpenAdapter");
    g_packet_set_loopback = (leap_packet_set_loopback_behavior_fn)GetProcAddress(
        g_packet_module,
        "PacketSetLoopbackBehavior");
    g_packet_close_adapter = (leap_packet_close_adapter_fn)GetProcAddress(
        g_packet_module,
        "PacketCloseAdapter");
}

static void leap_winpcap_set_loopback_capture(const char* device_name, int mode)
{
    leap_packet_adapter adapter;

    if (device_name == NULL || device_name[0] == '\0' ||
        g_packet_open_adapter == NULL || g_packet_set_loopback == NULL ||
        g_packet_close_adapter == NULL)
    {
        return;
    }

    adapter = g_packet_open_adapter((char*)device_name);
    if (adapter == NULL)
    {
        return;
    }

    (void)g_packet_set_loopback(adapter, mode);
    g_packet_close_adapter(adapter);
}

static void leap_winpcap_enable_loopback_capture(const char* device_name)
{
    leap_winpcap_set_loopback_capture(device_name, NPF_ENABLE_LOOPBACK);
}

#define LEAP_WIN_RELAY_DEPTH      32
#define LEAP_WIN_RELAY_FRAME_MAX  1600u

typedef struct LeapWinLoopbackRelayEntry
{
    uint8_t data[LEAP_WIN_RELAY_FRAME_MAX];
    size_t  length;
} LeapWinLoopbackRelayEntry;

typedef struct LeapWinLoopbackRelay
{
    CRITICAL_SECTION           lock;
    int                        initialized;
    LeapWinLoopbackRelayEntry  entries[LEAP_WIN_RELAY_DEPTH];
    int                        head;
    int                        count;
} LeapWinLoopbackRelay;

static LeapWinLoopbackRelay g_loopback_relay;

static int leap_winpcap_leap_header_length(
    const uint8_t* packet,
    size_t           caplen,
    size_t*          header_length_out)
{
    uint16_t ethertype;

    if (packet == NULL || header_length_out == NULL || caplen < 14u)
    {
        return 0;
    }

    ethertype = (uint16_t)(((uint16_t)packet[12] << 8) | (uint16_t)packet[13]);
    if (ethertype == 0x8100u)
    {
        if (caplen < 18u)
        {
            return 0;
        }

        ethertype = (uint16_t)(((uint16_t)packet[16] << 8) | (uint16_t)packet[17]);
        *header_length_out = 18u;
    }
    else
    {
        *header_length_out = 14u;
    }

    return (ethertype == (uint16_t)LEAP_ETHERTYPE_DEVELOPMENT ||
            ethertype == (uint16_t)LEAP_ETHERTYPE_EXPERIMENTAL_ALT ||
            ethertype == 0xB688u ||
            ethertype == 0xB587u) ? 1 : 0;
}

static void leap_winpcap_tune_capture_handle(pcap_t* handle, int physical_nic)
{
    (void)physical_nic;

    if (handle == NULL)
    {
        return;
    }

    if (g_winpcap.set_immediate_mode != NULL)
    {
        (void)g_winpcap.set_immediate_mode(handle, 1);
    }

    if (g_winpcap.setbuff != NULL)
    {
        (void)g_winpcap.setbuff(handle, 1024 * 1024);
    }

    if (g_winpcap.setmintocopy != NULL)
    {
        (void)g_winpcap.setmintocopy(handle, 0);
    }

    if (g_winpcap.set_tstamp_type != NULL)
    {
        static const int k_tstamp_prefs[] = {
            LEAP_PCAP_TSTAMP_HOST_HIPREC,
            LEAP_PCAP_TSTAMP_ADAPTER,
        };
        unsigned ti;

        for (ti = 0u; ti < (sizeof(k_tstamp_prefs) / sizeof(k_tstamp_prefs[0])); ti++)
        {
            if (g_winpcap.set_tstamp_type(handle, k_tstamp_prefs[ti]) == 0)
            {
                break;
            }
        }
    }
}

static int leap_winpcap_is_loopback_device(const char* device_name)
{
    if (device_name == NULL)
    {
        return 0;
    }

    return (strstr(device_name, "Loopback") != NULL) ? 1 : 0;
}

static void leap_winpcap_relay_init(void)
{
    if (g_loopback_relay.initialized != 0)
    {
        return;
    }

    InitializeCriticalSection(&g_loopback_relay.lock);
    g_loopback_relay.initialized = 1;
}

static void leap_winpcap_relay_push(const uint8_t* frame, size_t frame_length)
{
    LeapWinLoopbackRelayEntry* entry;
    int                        tail;

    if (frame == NULL || frame_length == 0u ||
        frame_length > LEAP_WIN_RELAY_FRAME_MAX)
    {
        return;
    }

    leap_winpcap_relay_init();
    EnterCriticalSection(&g_loopback_relay.lock);

    if (g_loopback_relay.count >= LEAP_WIN_RELAY_DEPTH)
    {
        g_loopback_relay.head =
            (g_loopback_relay.head + 1) % LEAP_WIN_RELAY_DEPTH;
        g_loopback_relay.count--;
    }

    tail = (g_loopback_relay.head + g_loopback_relay.count) %
           LEAP_WIN_RELAY_DEPTH;
    entry = &g_loopback_relay.entries[tail];
    memcpy(entry->data, frame, frame_length);
    entry->length = frame_length;
    g_loopback_relay.count++;

    LeaveCriticalSection(&g_loopback_relay.lock);
}

static int leap_winpcap_relay_pop(
    uint8_t* src_mac,
    uint8_t* payload,
    size_t   payload_capacity,
    size_t*  payload_length)
{
    LeapWinLoopbackRelayEntry* entry;
    size_t                     leap_len;

    if (payload == NULL || payload_length == NULL)
    {
        return -1;
    }

    *payload_length = 0u;

    if (g_loopback_relay.initialized == 0)
    {
        return -1;
    }

    EnterCriticalSection(&g_loopback_relay.lock);

    if (g_loopback_relay.count == 0)
    {
        LeaveCriticalSection(&g_loopback_relay.lock);
        return -1;
    }

    entry = &g_loopback_relay.entries[g_loopback_relay.head];
    if (entry->length < 14u)
    {
        g_loopback_relay.head =
            (g_loopback_relay.head + 1) % LEAP_WIN_RELAY_DEPTH;
        g_loopback_relay.count--;
        LeaveCriticalSection(&g_loopback_relay.lock);
        return -1;
    }

    leap_len = entry->length - 14u;
    if (leap_len > payload_capacity)
    {
        LeaveCriticalSection(&g_loopback_relay.lock);
        return -1;
    }

    if (src_mac != NULL)
    {
        memcpy(src_mac, entry->data + 6, 6);
    }

    memcpy(payload, entry->data + 14, leap_len);
    *payload_length = leap_len;

    g_loopback_relay.head =
        (g_loopback_relay.head + 1) % LEAP_WIN_RELAY_DEPTH;
    g_loopback_relay.count--;

    LeaveCriticalSection(&g_loopback_relay.lock);
    return 0;
}

static int leap_winpcap_load_api(void)
{
    char sysdir[MAX_PATH];
    char dll_path[MAX_PATH + 32];

    if (g_winpcap.module != NULL)
    {
        return 0;
    }

    if (GetSystemDirectoryA(sysdir, MAX_PATH) != 0)
    {
        (void)snprintf(
            dll_path,
            sizeof(dll_path),
            "%s\\Npcap\\Packet.dll",
            sysdir);
        (void)LoadLibraryA(dll_path);

        (void)snprintf(
            dll_path,
            sizeof(dll_path),
            "%s\\Npcap\\wpcap.dll",
            sysdir);
        if (leap_winpcap_try_load(dll_path) == 0)
        {
            goto loaded;
        }
    }

    if (leap_winpcap_try_load("wpcap.dll") != 0 &&
        leap_winpcap_try_load("Npcap\\wpcap.dll") != 0)
    {
        (void)snprintf(
            g_winpcap_last_errbuf,
            sizeof(g_winpcap_last_errbuf),
            "wpcap.dll not found (win32 err=%lu) - install Npcap from https://npcap.com/",
            (unsigned long)GetLastError());
        leap_winpcap_set_errno((int)GetLastError());
        return -1;
    }

loaded:

#define LOAD_REQ(name)                                                  \
    g_winpcap.name = (leap_pcap_##name##_fn)GetProcAddress(             \
        g_winpcap.module, "pcap_" #name);                               \
    if (g_winpcap.name == NULL)                                         \
    {                                                                   \
        (void)snprintf(                                                 \
            g_winpcap_last_errbuf,                                      \
            sizeof(g_winpcap_last_errbuf),                              \
            "missing export pcap_" #name " (err=%lu)",                  \
            (unsigned long)GetLastError());                             \
        leap_winpcap_set_errno((int)GetLastError());                    \
        return -1;                                                      \
    }

#define LOAD_OPT(name)                                                  \
    g_winpcap.name = (leap_pcap_##name##_fn)GetProcAddress(             \
        g_winpcap.module, "pcap_" #name);

    LOAD_REQ(open_live);
    LOAD_REQ(close);
    LOAD_REQ(sendpacket);
    LOAD_REQ(next_ex);
    LOAD_OPT(compile);
    LOAD_OPT(setfilter);
    LOAD_OPT(freecode);
    LOAD_OPT(findalldevs);
    LOAD_OPT(freealldevs);
    LOAD_OPT(setmintocopy);
    LOAD_OPT(set_immediate_mode);
    LOAD_OPT(setbuff);
    LOAD_OPT(setnonblock);
    LOAD_OPT(setdirection);
    LOAD_OPT(set_tstamp_type);

#undef LOAD_REQ
#undef LOAD_OPT

    leap_winpcap_load_packet_dll();

    return 0;
}

uint64_t leap_raw_winpcap_monotonic_us(void)
{
#if defined(_WIN32) && !defined(LEAP_RAW_WINPCAP_STUB)
    return leap_win_monotonic_us();
#else
    return 0u;
#endif
}

static int leap_winpcap_pick_loopback(char* out, size_t out_capacity)
{
    static const char k_npcap_loopback[] = "\\Device\\NPF_Loopback";

    if (out == NULL || out_capacity == 0u)
    {
        return -1;
    }

    (void)snprintf(out, out_capacity, "%s", k_npcap_loopback);
    return 0;
}

static int leap_winpcap_mac_from_adapter_info(const char* guid, uint8_t* mac_out)
{
    ULONG              size = 0u;
    PIP_ADAPTER_INFO info = NULL;
    PIP_ADAPTER_INFO cur;
    ULONG              rc;

    if (guid == NULL || mac_out == NULL || guid[0] == '\0')
    {
        return -1;
    }

    rc = GetAdaptersInfo(NULL, &size);
    if (rc != ERROR_BUFFER_OVERFLOW || size == 0u)
    {
        return -1;
    }

    info = (PIP_ADAPTER_INFO)HeapAlloc(GetProcessHeap(), 0, size);
    if (info == NULL)
    {
        return -1;
    }

    rc = GetAdaptersInfo(info, &size);
    if (rc != NO_ERROR)
    {
        HeapFree(GetProcessHeap(), 0, info);
        return -1;
    }

    for (cur = info; cur != NULL; cur = cur->Next)
    {
        if (_stricmp(cur->AdapterName, guid) != 0)
        {
            continue;
        }

        if (cur->AddressLength != 6u)
        {
            continue;
        }

        memcpy(mac_out, cur->Address, 6);
        HeapFree(GetProcessHeap(), 0, info);
        return 0;
    }

    HeapFree(GetProcessHeap(), 0, info);
    return -1;
}

static int leap_winpcap_mac_from_guid(const char* guid, uint8_t* mac_out)
{
    ULONG                 size = 0u;
    PIP_ADAPTER_ADDRESSES addrs = NULL;
    PIP_ADAPTER_ADDRESSES cur;
    ULONG                 rc;

    if (guid == NULL || mac_out == NULL || guid[0] == '\0')
    {
        return -1;
    }

    rc = GetAdaptersAddresses(
        AF_UNSPEC,
        LEAP_WINPCAP_GAA_FLAGS,
        NULL,
        NULL,
        &size);
    if (rc != ERROR_BUFFER_OVERFLOW || size == 0u)
    {
        return -1;
    }

    addrs = (PIP_ADAPTER_ADDRESSES)HeapAlloc(GetProcessHeap(), 0, size);
    if (addrs == NULL)
    {
        return -1;
    }

    rc = GetAdaptersAddresses(
        AF_UNSPEC,
        LEAP_WINPCAP_GAA_FLAGS,
        NULL,
        addrs,
        &size);
    if (rc != NO_ERROR)
    {
        HeapFree(GetProcessHeap(), 0, addrs);
        return -1;
    }

    for (cur = addrs; cur != NULL; cur = cur->Next)
    {
        if (cur->AdapterName == NULL ||
            _stricmp(cur->AdapterName, guid) != 0)
        {
            continue;
        }

        if (cur->PhysicalAddressLength != 6u)
        {
            continue;
        }

        memcpy(mac_out, cur->PhysicalAddress, 6);
        HeapFree(GetProcessHeap(), 0, addrs);
        return 0;
    }

    HeapFree(GetProcessHeap(), 0, addrs);
    return -1;
}

static int leap_winpcap_extract_guid(
    const char* device_name,
    char*       guid_out,
    size_t      guid_capacity)
{
    const char* start;
    const char* end;
    size_t      len;

    if (device_name == NULL || guid_out == NULL || guid_capacity == 0u)
    {
        return -1;
    }

    start = strchr(device_name, '{');
    if (start == NULL)
    {
        return -1;
    }

    end = strchr(start, '}');
    if (end == NULL)
    {
        return -1;
    }

    len = (size_t)(end - start + 1);
    if (len + 1u > guid_capacity)
    {
        return -1;
    }

    memcpy(guid_out, start, len);
    guid_out[len] = '\0';
    return 0;
}

static int leap_winpcap_resolve_mac(
    const char* device_name,
    uint8_t*    mac_out)
{
    char guid[64];

    if (mac_out == NULL)
    {
        return -1;
    }

    if (device_name != NULL &&
        leap_winpcap_is_loopback_device(device_name) != 0)
    {
        memcpy(mac_out, k_npcap_loopback_mac, 6);
        return 0;
    }

    if (device_name != NULL &&
        leap_winpcap_extract_guid(device_name, guid, sizeof(guid)) == 0)
    {
        if (leap_winpcap_mac_from_guid(guid, mac_out) == 0)
        {
            return 0;
        }

        if (leap_winpcap_mac_from_adapter_info(guid, mac_out) == 0)
        {
            return 0;
        }
    }

    if (device_name != NULL &&
        leap_winpcap_is_loopback_device(device_name) != 0)
    {
        memcpy(mac_out, k_npcap_loopback_mac, 6);
        return 0;
    }

    return -1;
}

void leap_raw_winpcap_list_devices(void)
{
    ULONG                 size = 0u;
    PIP_ADAPTER_ADDRESSES addrs = NULL;
    PIP_ADAPTER_ADDRESSES cur;
    ULONG                 rc;

    printf("  \\Device\\NPF_Loopback (Npcap Loopback - default for local smoke)\n");

    rc = GetAdaptersAddresses(
        AF_UNSPEC,
        LEAP_WINPCAP_GAA_FLAGS,
        NULL,
        NULL,
        &size);
    if (rc != ERROR_BUFFER_OVERFLOW || size == 0u)
    {
        printf("  hint: install Npcap from https://npcap.com/\n");
        return;
    }

    addrs = (PIP_ADAPTER_ADDRESSES)HeapAlloc(GetProcessHeap(), 0, size);
    if (addrs == NULL)
    {
        return;
    }

    rc = GetAdaptersAddresses(
        AF_UNSPEC,
        LEAP_WINPCAP_GAA_FLAGS,
        NULL,
        addrs,
        &size);
    if (rc != NO_ERROR)
    {
        HeapFree(GetProcessHeap(), 0, addrs);
        return;
    }

    for (cur = addrs; cur != NULL; cur = cur->Next)
    {
        if (cur->AdapterName == NULL || cur->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
        {
            continue;
        }

        printf("  \\Device\\NPF_%s", cur->AdapterName);
        if (cur->FriendlyName != NULL && cur->FriendlyName[0] != L'\0')
        {
            printf("  (");
            (void)fputws(cur->FriendlyName, stdout);
            printf(")");
        }
        if (cur->PhysicalAddressLength == 6u)
        {
            printf("  %02x:%02x:%02x:%02x:%02x:%02x",
                   cur->PhysicalAddress[0],
                   cur->PhysicalAddress[1],
                   cur->PhysicalAddress[2],
                   cur->PhysicalAddress[3],
                   cur->PhysicalAddress[4],
                   cur->PhysicalAddress[5]);
        }
        printf("\n");
    }

    HeapFree(GetProcessHeap(), 0, addrs);
    printf("  hint: match InterfaceGuid from Get-NetAdapter to NPF_{GUID}\n");
}

int leap_raw_winpcap_open(
    LeapRawWinpcapSocket*             sock,
    const char*                       device_name,
    uint16_t                          ethertype,
    const LeapRawWinpcapOpenOptions* options)
{
    char         chosen[LEAP_RAW_WINPCAP_NAME_MAX];
    char         errbuf[PCAP_ERRBUF_SIZE];
    pcap_t*      handle;
    int          promiscuous = 1;
    int          filter_leap = 1;
    bpf_program  fp;

    if (sock == NULL)
    {
        return -1;
    }

    if (leap_winpcap_load_api() != 0)
    {
        if (g_winpcap_last_errbuf[0] == '\0')
        {
            (void)snprintf(
                g_winpcap_last_errbuf,
                sizeof(g_winpcap_last_errbuf),
                "wpcap.dll load failed (win32 err=%d)",
                g_winpcap_last_errno);
        }
        return -1;
    }

    memset(sock, 0, sizeof(*sock));
    sock->cached_link_up = -1;
    sock->ethertype      = ethertype;

    if (options != NULL)
    {
        promiscuous = options->promiscuous;
        filter_leap = options->filter_leap_ethertype;
    }

    sock->promiscuous      = (promiscuous != 0) ? 1 : 0;
    sock->filter_ethertype = (filter_leap != 0) ? 1 : 0;

    if (device_name == NULL || device_name[0] == '\0')
    {
        if (leap_winpcap_pick_loopback(chosen, sizeof(chosen)) != 0)
        {
            return -1;
        }
    }
    else
    {
        (void)snprintf(chosen, sizeof(chosen), "%s", device_name);
    }

    (void)snprintf(sock->device_name, sizeof(sock->device_name), "%s", chosen);

    if (leap_winpcap_resolve_mac(chosen, sock->local_mac) != 0)
    {
        (void)snprintf(
            g_winpcap_last_errbuf,
            sizeof(g_winpcap_last_errbuf),
            "could not resolve MAC for adapter '%s'",
            chosen);
        return -1;
    }

    if (leap_winpcap_is_loopback_device(chosen) != 0)
    {
        leap_winpcap_enable_loopback_capture(chosen);
    }
    else
    {
        leap_winpcap_set_loopback_capture(chosen, NPF_DISABLE_LOOPBACK);
    }

    /*
     * Physical NIC: honor --promisc. Software filters drop TX echoes.
     */
    handle = g_winpcap.open_live(
        chosen,
        65536,
        (promiscuous != 0) ? 1 : 0,
        100,
        errbuf);
    if (handle == NULL)
    {
        (void)snprintf(
            g_winpcap_last_errbuf,
            sizeof(g_winpcap_last_errbuf),
            "%s",
            errbuf);
        fprintf(
            stderr,
            "pcap_open_live(%s) failed: %s (run as Admin; Npcap required)\n",
            chosen,
            errbuf);
        return -1;
    }

    if (filter_leap != 0 &&
        leap_winpcap_is_loopback_device(chosen) != 0 &&
        g_winpcap.compile != NULL &&
        g_winpcap.setfilter != NULL &&
        g_winpcap.freecode != NULL)
    {
        if (g_winpcap.compile(
                handle,
                &fp,
                "ether proto 0x88b6 or ether proto 0x88b5",
                1,
                PCAP_NETMASK_UNKNOWN) == 0)
        {
            (void)g_winpcap.setfilter(handle, &fp);
            g_winpcap.freecode(&fp);
        }
    }

    /* Physical NIC: rely on software LEAP/unicast filters (no kernel BPF). */
    leap_winpcap_tune_capture_handle(
        handle,
        leap_winpcap_is_loopback_device(chosen) == 0 ? 1 : 0);

    sock->pcap = handle;

    leap_winpcap_clear_errno();
    return 0;
}

void leap_raw_winpcap_close(LeapRawWinpcapSocket* sock)
{
    if (sock == NULL)
    {
        return;
    }

    if (sock->pcap_tx != NULL)
    {
        g_winpcap.close((pcap_t*)sock->pcap_tx);
        sock->pcap_tx = NULL;
    }

    if (sock->pcap == NULL)
    {
        return;
    }

    g_winpcap.close((pcap_t*)sock->pcap);
    sock->pcap = NULL;
}

int leap_raw_winpcap_send(
    LeapRawWinpcapSocket* sock,
    const uint8_t*        dst_mac,
    const uint8_t*        payload,
    size_t                payload_length)
{
    uint8_t frame[LEAP_MAX_ETHERNET_PAYLOAD + 14u];
    size_t  total;
    size_t  wire_total;

    if (sock == NULL || sock->pcap == NULL || dst_mac == NULL || payload == NULL)
    {
        return -1;
    }

    if (payload_length > LEAP_MAX_ETHERNET_PAYLOAD)
    {
        return -1;
    }

    total = 14u + payload_length;
    wire_total = total < 60u ? 60u : total;
    memcpy(frame, dst_mac, 6);
    memcpy(frame + 6, sock->local_mac, 6);
    /* Ethernet II type field is big-endian on the wire. */
    frame[12] = (uint8_t)((sock->ethertype >> 8) & 0xFFu);
    frame[13] = (uint8_t)(sock->ethertype & 0xFFu);
    memcpy(frame + 14, payload, payload_length);
    if (wire_total > total)
    {
        memset(frame + total, 0, wire_total - total);
    }

    {
        pcap_t* tx_handle = (pcap_t*)sock->pcap;

        if (g_winpcap.sendpacket(tx_handle, frame, (int)wire_total) != 0)
        {
            sock->stats.tx_errors++;
            leap_winpcap_set_errno((int)GetLastError());
            return -1;
        }
    }

    if (leap_winpcap_is_loopback_device(sock->device_name) != 0)
    {
        leap_winpcap_relay_push(frame, wire_total);
    }

    sock->stats.tx_frames_ok++;
    sock->stats.tx_bytes += wire_total;
    leap_winpcap_clear_errno();
    return 0;
}

static int leap_winpcap_is_broadcast_mac(const uint8_t* mac)
{
    static const uint8_t k_bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    if (mac == NULL)
    {
        return 0;
    }

    return (memcmp(mac, k_bcast, 6) == 0) ? 1 : 0;
}

static int leap_winpcap_process_packet(
    LeapRawWinpcapSocket*     sock,
    const struct pcap_pkthdr* header,
    const u_char*             packet,
    uint8_t*                  src_mac,
    uint8_t*                  payload,
    size_t                    payload_capacity,
    size_t*                   payload_length)
{
    size_t header_length = 14u;

    if (header->caplen < 14u)
    {
        sock->stats.rx_short_frames++;
        return 0;
    }

    if (sock->filter_ethertype != 0 &&
        leap_winpcap_leap_header_length(
            packet,
            header->caplen,
            &header_length) == 0)
    {
        sock->stats.rx_filtered++;
        return 0;
    }

    if (leap_winpcap_is_loopback_device(sock->device_name) == 0 &&
        memcmp(packet + 6, sock->local_mac, 6) == 0)
    {
        sock->stats.rx_filtered++;
        return 0;
    }

    /*
     * Npcap on physical NICs often delivers copies of our own broadcast TX
     * (HELLO) with a non-local source address. Drop broadcast destinations
     * so discovery sees unicast HELLO_REPLY frames only.
     */
    if (leap_winpcap_is_loopback_device(sock->device_name) == 0 &&
        leap_winpcap_is_broadcast_mac(packet) != 0)
    {
        sock->stats.rx_filtered++;
        return 0;
    }

    if (leap_winpcap_is_loopback_device(sock->device_name) == 0 &&
        memcmp(packet, sock->local_mac, 6) != 0)
    {
        sock->stats.rx_filtered++;
        return 0;
    }

    if (src_mac != NULL)
    {
        memcpy(src_mac, packet + 6, 6);
    }

    {
        size_t leap_len = header->caplen - header_length;

        if (leap_len > payload_capacity)
        {
            sock->stats.rx_errors++;
            return -1;
        }

        memcpy(payload, packet + header_length, leap_len);
        *payload_length = leap_len;

        sock->last_rx_valid_leap   = 0;
        sock->last_rx_payload_len  = leap_len;
        sock->last_rx_eth_caplen   = header->caplen;
        if (leap_len > sizeof(sock->last_rx_payload))
        {
            memcpy(sock->last_rx_payload, packet + header_length,
                   sizeof(sock->last_rx_payload));
        }
        else if (leap_len > 0u)
        {
            memcpy(sock->last_rx_payload, packet + header_length, leap_len);
        }

        if (leap_len >= 12u &&
            sock->last_rx_payload[0] == 'L' &&
            sock->last_rx_payload[1] == 'E' &&
            sock->last_rx_payload[2] == 'A' &&
            sock->last_rx_payload[3] == 'P')
        {
            sock->last_rx_valid_leap   = 1;
            sock->last_rx_service_id   =
                (uint16_t)((uint16_t)sock->last_rx_payload[8] |
                            ((uint16_t)sock->last_rx_payload[9] << 8));
            sock->last_rx_message_type =
                (uint16_t)((uint16_t)sock->last_rx_payload[10] |
                            ((uint16_t)sock->last_rx_payload[11] << 8));
        }
    }

    sock->stats.rx_frames_ok++;
    sock->stats.rx_bytes += *payload_length;
    leap_winpcap_clear_errno();
    return 1;
}

static uint64_t leap_winpcap_pkthdr_us(const struct pcap_pkthdr* header)
{
    if (header == NULL)
    {
        return 0u;
    }

    return ((uint64_t)(unsigned long)header->ts.tv_sec * 1000000u) +
           (uint64_t)(unsigned long)header->ts.tv_usec;
}

static uint64_t leap_winpcap_capture_mono_us(
    LeapRawWinpcapSocket*     sock,
    const struct pcap_pkthdr* header)
{
    uint64_t pcap_us;
    uint64_t mono_now;
    int64_t  delta_us;
    int64_t  mapped_us;

    if (sock == NULL || header == NULL)
    {
        return leap_win_monotonic_us();
    }

    pcap_us  = leap_winpcap_pkthdr_us(header);
    mono_now = leap_win_monotonic_us();

    if (sock->capture_time_synced == 0)
    {
        sock->capture_base_pcap_us   = pcap_us;
        sock->capture_base_mono_us   = mono_now;
        sock->capture_time_synced    = 1;
        return mono_now;
    }

    delta_us = (int64_t)pcap_us - (int64_t)sock->capture_base_pcap_us;
    mapped_us =
        (int64_t)sock->capture_base_mono_us + delta_us;
    if (mapped_us < 0)
    {
        return mono_now;
    }

    /*
     * Default HOST timestamps on some Windows/Npcap NIC paths quantize to
     * ~10 ms. Mapped pcap time can sit far ahead of QPC at read time; clamp
     * so wire recv never exceeds when userspace actually read the packet.
     */
    if ((uint64_t)mapped_us > mono_now)
    {
        return mono_now;
    }

    return (uint64_t)mapped_us;
}

static int leap_winpcap_next_ex_any(
    LeapRawWinpcapSocket* sock,
    struct pcap_pkthdr**  header_out,
    const u_char**        packet_out,
    int                   timeout_ms)
{
    (void)timeout_ms;

    return g_winpcap.next_ex((pcap_t*)sock->pcap, header_out, packet_out);
}

/*
 * Non-blocking pcap read for timeout_ms==0 polls. Without this, the 100 ms
 * pcap_open_live read timeout makes "poll" calls block one full interval.
 */
static int leap_winpcap_try_next_ex(
    LeapRawWinpcapSocket* sock,
    struct pcap_pkthdr**  header_out,
    const u_char**        packet_out)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    int  nonblock_set = 0;
    int  result;

    if (sock == NULL || sock->pcap == NULL)
    {
        return -1;
    }

    if (g_winpcap.setnonblock != NULL)
    {
        if (g_winpcap.setnonblock((pcap_t*)sock->pcap, 1, errbuf) == 0)
        {
            nonblock_set = 1;
        }
    }

    result = g_winpcap.next_ex((pcap_t*)sock->pcap, header_out, packet_out);

    if (nonblock_set != 0)
    {
        (void)g_winpcap.setnonblock((pcap_t*)sock->pcap, 0, errbuf);
    }

    return result;
}

int leap_raw_winpcap_recv(
    LeapRawWinpcapSocket* sock,
    uint8_t*              src_mac,
    uint8_t*              payload,
    size_t                payload_capacity,
    size_t*               payload_length,
    int                   timeout_ms,
    uint64_t*             capture_mono_us_out)
{
    struct pcap_pkthdr* header;
    const u_char*      packet;
    int                result;
    uint64_t           deadline_ms;
    uint64_t           now_ms;

    if (sock == NULL || sock->pcap == NULL || payload == NULL ||
        payload_length == NULL)
    {
        return -1;
    }

    *payload_length = 0u;
    if (capture_mono_us_out != NULL)
    {
        *capture_mono_us_out = 0u;
    }

    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }

    now_ms      = (uint64_t)GetTickCount64();
    deadline_ms = now_ms + (uint64_t)timeout_ms;

    if (timeout_ms == 0)
    {
        for (;;)
        {
            if (leap_winpcap_is_loopback_device(sock->device_name) != 0 &&
                leap_winpcap_relay_pop(
                    src_mac,
                    payload,
                    payload_capacity,
                    payload_length) == 0)
            {
                sock->stats.rx_frames_ok++;
                sock->stats.rx_bytes += *payload_length;
                leap_winpcap_clear_errno();
                if (capture_mono_us_out != NULL)
                {
                    *capture_mono_us_out = leap_win_monotonic_us();
                }
                return 0;
            }

            result = leap_winpcap_try_next_ex(sock, &header, &packet);
            if (result == 0)
            {
                leap_winpcap_clear_errno();
                return -1;
            }

            if (result < 0)
            {
                sock->stats.rx_errors++;
                return -1;
            }

            if (header == NULL)
            {
                sock->stats.rx_errors++;
                return -1;
            }

            result = leap_winpcap_process_packet(
                sock,
                header,
                packet,
                src_mac,
                payload,
                payload_capacity,
                payload_length);
            if (result == 1)
            {
                if (capture_mono_us_out != NULL)
                {
                    *capture_mono_us_out =
                        leap_winpcap_capture_mono_us(sock, header);
                }
                return 0;
            }

            if (result < 0)
            {
                return -1;
            }
        }
    }

    for (;;)
    {
        if (leap_winpcap_is_loopback_device(sock->device_name) != 0 &&
            leap_winpcap_relay_pop(
                src_mac,
                payload,
                payload_capacity,
                payload_length) == 0)
        {
            sock->stats.rx_frames_ok++;
            sock->stats.rx_bytes += *payload_length;
            leap_winpcap_clear_errno();
            if (capture_mono_us_out != NULL)
            {
                *capture_mono_us_out = leap_win_monotonic_us();
            }
            return 0;
        }

        result = leap_winpcap_next_ex_any(sock, &header, &packet, timeout_ms);
        if (result == 0)
        {
            sock->stats.rx_timeouts++;
            leap_winpcap_clear_errno();

            if (timeout_ms == 0)
            {
                return -1;
            }

            now_ms = (uint64_t)GetTickCount64();
            if (now_ms >= deadline_ms)
            {
                return -1;
            }

            continue;
        }

        if (result < 0)
        {
            sock->stats.rx_errors++;
            return -1;
        }

        if (header == NULL)
        {
            sock->stats.rx_errors++;
            return -1;
        }

        result = leap_winpcap_process_packet(
            sock,
            header,
            packet,
            src_mac,
            payload,
            payload_capacity,
            payload_length);
        if (result == 1)
        {
            if (capture_mono_us_out != NULL)
            {
                *capture_mono_us_out =
                    leap_winpcap_capture_mono_us(sock, header);
            }
            return 0;
        }

        if (result < 0)
        {
            return -1;
        }
    }
}

void leap_raw_winpcap_get_stats(
    const LeapRawWinpcapSocket* sock,
    LeapRawWinpcapStats*        out)
{
    if (sock == NULL || out == NULL)
    {
        return;
    }

    *out = sock->stats;
}

void leap_raw_winpcap_reset_stats(LeapRawWinpcapSocket* sock)
{
    if (sock == NULL)
    {
        return;
    }

    memset(&sock->stats, 0, sizeof(sock->stats));
}

int leap_raw_winpcap_query_link(
    const LeapRawWinpcapSocket* sock,
    LeapRawWinpcapLinkState*    state_out)
{
    if (sock == NULL || state_out == NULL)
    {
        return -1;
    }

    state_out->interface_up = (sock->pcap != NULL) ? 1 : 0;
    state_out->link_up      = state_out->interface_up;
    return 0;
}

int leap_raw_winpcap_poll_link(
    LeapRawWinpcapSocket*    sock,
    int*                     changed_out,
    LeapRawWinpcapLinkState* state_out)
{
    LeapRawWinpcapLinkState state;
    int                     prev;
    int                     changed = 0;

    if (sock == NULL)
    {
        return -1;
    }

    if (changed_out != NULL)
    {
        *changed_out = 0;
    }

    if (leap_raw_winpcap_query_link(sock, &state) != 0)
    {
        return -1;
    }

    if (state_out != NULL)
    {
        *state_out = state;
    }

    prev = sock->cached_link_up;
    if (prev >= 0 && state.link_up != prev)
    {
        changed = 1;
        sock->stats.link_transitions++;
    }

    sock->cached_link_up = state.link_up;

    if (changed_out != NULL)
    {
        *changed_out = changed;
    }

    return 0;
}

#else

int leap_raw_winpcap_last_errno(void)
{
    return 0;
}

uint64_t leap_raw_winpcap_monotonic_us(void)
{
    return 0u;
}

const char* leap_raw_winpcap_last_error(void)
{
    return "";
}

void leap_raw_winpcap_list_devices(void)
{
}

int leap_raw_winpcap_open(
    LeapRawWinpcapSocket*             sock,
    const char*                       device_name,
    uint16_t                          ethertype,
    const LeapRawWinpcapOpenOptions* options)
{
    (void)sock;
    (void)device_name;
    (void)ethertype;
    (void)options;
    return -1;
}

void leap_raw_winpcap_close(LeapRawWinpcapSocket* sock)
{
    (void)sock;
}

int leap_raw_winpcap_send(
    LeapRawWinpcapSocket* sock,
    const uint8_t*        dst_mac,
    const uint8_t*        payload,
    size_t                payload_length)
{
    (void)sock;
    (void)dst_mac;
    (void)payload;
    (void)payload_length;
    return -1;
}

int leap_raw_winpcap_recv(
    LeapRawWinpcapSocket* sock,
    uint8_t*              src_mac,
    uint8_t*              payload,
    size_t                payload_capacity,
    size_t*               payload_length,
    int                   timeout_ms,
    uint64_t*             capture_mono_us_out)
{
    (void)sock;
    (void)src_mac;
    (void)payload;
    (void)payload_capacity;
    (void)payload_length;
    (void)timeout_ms;
    (void)capture_mono_us_out;
    return -1;
}

void leap_raw_winpcap_get_stats(
    const LeapRawWinpcapSocket* sock,
    LeapRawWinpcapStats*        out)
{
    (void)sock;
    if (out != NULL)
    {
        memset(out, 0, sizeof(*out));
    }
}

void leap_raw_winpcap_reset_stats(LeapRawWinpcapSocket* sock)
{
    (void)sock;
}

int leap_raw_winpcap_query_link(
    const LeapRawWinpcapSocket* sock,
    LeapRawWinpcapLinkState*    state_out)
{
    (void)sock;
    if (state_out != NULL)
    {
        memset(state_out, 0, sizeof(*state_out));
    }
    return -1;
}

int leap_raw_winpcap_poll_link(
    LeapRawWinpcapSocket*    sock,
    int*                     changed_out,
    LeapRawWinpcapLinkState* state_out)
{
    (void)sock;
    if (changed_out != NULL)
    {
        *changed_out = 0;
    }
    if (state_out != NULL)
    {
        memset(state_out, 0, sizeof(*state_out));
    }
    return -1;
}

#endif /* _WIN32 */
