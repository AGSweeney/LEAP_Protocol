/*******************************************************************************
 * Helpers for converting between HAL endpoints and sockaddr_in used by encap.
 ******************************************************************************/

#ifndef OPENER_NET_GLUE_H_
#define OPENER_NET_GLUE_H_

#include <string.h>

#include "opener_hal_types.h"
#include "opener_sockaddr.h"

static inline void OpenerEndpointToSockaddrIn(const OpenerHalEndpoint *endpoint,
                                              struct sockaddr_in *address_out) {
  if((NULL == endpoint) || (NULL == address_out)) {
    return;
  }
  memset(address_out, 0, sizeof(*address_out));
  address_out->sin_family = AF_INET;
  address_out->sin_port = htons(endpoint->port);
  address_out->sin_addr.s_addr = endpoint->address;
}

static inline void OpenerSockaddrInToEndpoint(const struct sockaddr_in *address_in,
                                              OpenerHalEndpoint *endpoint_out) {
  if((NULL == address_in) || (NULL == endpoint_out)) {
    return;
  }
  endpoint_out->port = ntohs(address_in->sin_port);
  endpoint_out->address = address_in->sin_addr.s_addr;
}

#endif /* OPENER_NET_GLUE_H_ */
