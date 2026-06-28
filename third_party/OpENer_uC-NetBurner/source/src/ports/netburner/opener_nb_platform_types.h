/*******************************************************************************
 * Minimal types shared by opener_nb_lldp / opener_nb_acd wire modules.
 ******************************************************************************/
#ifndef OPENER_NB_PLATFORM_TYPES_H_
#define OPENER_NB_PLATFORM_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum opener_nb_status {
  OPENER_NB_OK = 0,
  OPENER_NB_ERR_INVALID_ARG = -1,
  OPENER_NB_ERR_NO_MEM = -2,
  OPENER_NB_ERR_TIMEOUT = -3,
  OPENER_NB_ERR_IO = -4,
  OPENER_NB_ERR_PROTOCOL = -5,
  OPENER_NB_ERR_NOT_FOUND = -6,
  OPENER_NB_ERR_BUSY = -7,
  OPENER_NB_ERR_NOT_SUPPORTED = -8,
  OPENER_NB_ERR_STATE = -9,
  OPENER_NB_ERR_CLOSED = -10
} opener_nb_status_t;

typedef struct opener_nb_ipv4 {
  uint8_t octets[4];
} opener_nb_ipv4_t;

static inline uint32_t opener_nb_ipv4_to_u32_be(opener_nb_ipv4_t ip)
{
  return ((uint32_t)ip.octets[0] << 24) |
         ((uint32_t)ip.octets[1] << 16) |
         ((uint32_t)ip.octets[2] << 8) |
         (uint32_t)ip.octets[3];
}

static inline opener_nb_ipv4_t opener_nb_ipv4_from_u32_be(uint32_t v)
{
  opener_nb_ipv4_t ip;
  ip.octets[0] = (uint8_t)((v >> 24) & 0xFFu);
  ip.octets[1] = (uint8_t)((v >> 16) & 0xFFu);
  ip.octets[2] = (uint8_t)((v >> 8) & 0xFFu);
  ip.octets[3] = (uint8_t)(v & 0xFFu);
  return ip;
}

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_PLATFORM_TYPES_H_ */
