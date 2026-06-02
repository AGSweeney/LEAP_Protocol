#ifndef BBB_CPSW_RAW_H
#define BBB_CPSW_RAW_H

#include <stddef.h>
#include <stdint.h>

#define BBB_CPSW_MAX_PAYLOAD 1536u

typedef struct BbbCpswRaw
{
    uint8_t mac[6];
    int     link_up;
} BbbCpswRaw;

int bbb_cpsw_raw_init(BbbCpswRaw* net);
int bbb_cpsw_raw_send(
    BbbCpswRaw*    net,
    const uint8_t* dst_mac,
    const uint8_t* payload,
    size_t         payload_length);
int bbb_cpsw_raw_recv(
    BbbCpswRaw* net,
    uint8_t*    src_mac,
    uint8_t*    payload,
    size_t      payload_capacity,
    size_t*     payload_length);
void bbb_cpsw_raw_debug_status(void);
void bbb_cpsw_raw_print_phy_bsr(void);
void bbb_cpsw_raw_note_peer(const uint8_t* mac);

#endif
