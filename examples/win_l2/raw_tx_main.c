/*
 * Minimal Windows raw Ethernet transmitter for bench bring-up.
 */

#include "leap/leap_raw_winpcap.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    const char* adapter = NULL;
    unsigned    count = 20u;
    uint16_t    ethertype = LEAP_ETHERTYPE_DEVELOPMENT;
    unsigned    i;
    LeapRawWinpcapSocket sock;
    LeapRawWinpcapOpenOptions opts;
    static const uint8_t bcast[6] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    uint8_t payload[46];

    if (argc > 1 && strcmp(argv[1], "--list") == 0)
    {
        leap_raw_winpcap_list_devices();
        return 0;
    }

    if (argc > 1)
    {
        adapter = argv[1];
    }
    if (argc > 2)
    {
        count = (unsigned)strtoul(argv[2], NULL, 0);
        if (count == 0u)
        {
            count = 1u;
        }
    }
    if (argc > 3)
    {
        ethertype = (uint16_t)strtoul(argv[3], NULL, 0);
    }

    memset(&opts, 0, sizeof(opts));
    opts.promiscuous = 1;
    opts.filter_leap_ethertype = 0;

    if (leap_raw_winpcap_open(&sock, adapter, ethertype, &opts) != 0)
    {
        fprintf(stderr, "open failed: %s\n", leap_raw_winpcap_last_error());
        return 1;
    }

    memset(payload, 0xA5, sizeof(payload));

    printf("raw tx on %s local %02x:%02x:%02x:%02x:%02x:%02x count=%u eth=0x%04x\n",
           sock.device_name,
           sock.local_mac[0],
           sock.local_mac[1],
           sock.local_mac[2],
           sock.local_mac[3],
           sock.local_mac[4],
           sock.local_mac[5],
           count,
           ethertype);

    for (i = 0u; i < count; i++)
    {
        if (leap_raw_winpcap_send(&sock, bcast, payload, sizeof(payload)) != 0)
        {
            fprintf(stderr, "send failed: %s\n", leap_raw_winpcap_last_error());
            leap_raw_winpcap_close(&sock);
            return 1;
        }
#if defined(_WIN32)
        Sleep(100u);
#endif
    }

    leap_raw_winpcap_close(&sock);
    printf("sent %u frames\n", count);
    return 0;
}
