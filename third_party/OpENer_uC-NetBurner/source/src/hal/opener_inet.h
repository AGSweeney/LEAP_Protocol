/*******************************************************************************
 * Portable IPv4 byte-order helpers — no socket headers required.
 ******************************************************************************/

#ifndef OPENER_INET_H_
#define OPENER_INET_H_

#include "typedefs.h"

static inline CipUint Opener_htons(CipUint host_short) {
  return (CipUint)((host_short >> 8) | ((host_short & 0xFFU) << 8));
}

static inline CipUdint Opener_htonl(CipUdint host_long) {
  return ((host_long & 0xFF000000UL) >> 24) |
         ((host_long & 0x00FF0000UL) >> 8) |
         ((host_long & 0x0000FF00UL) << 8) |
         ((host_long & 0x000000FFUL) << 24);
}

static inline CipUint Opener_ntohs(CipUint net_short) {
  return Opener_htons(net_short);
}

static inline CipUdint Opener_ntohl(CipUdint net_long) {
  return Opener_htonl(net_long);
}

#define htons Opener_htons
#define htonl Opener_htonl
#define ntohs Opener_ntohs
#define ntohl Opener_ntohl

#ifndef AF_INET
#define AF_INET 2
#endif

#ifndef INADDR_ANY
#define INADDR_ANY 0U
#endif

typedef CipUint in_port_t;

#endif /* OPENER_INET_H_ */
