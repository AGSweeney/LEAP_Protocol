#ifndef LEAP_DEVICE_FIRMWARE_LWIP_HOOKS_H_
#define LEAP_DEVICE_FIRMWARE_LWIP_HOOKS_H_

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t LeapDevice_LwipUnknownEthProtocolHook(struct pbuf *packet, struct netif *netif);

#ifdef __cplusplus
}
#endif

#define LWIP_HOOK_UNKNOWN_ETH_PROTOCOL(p, netif) \
    LeapDevice_LwipUnknownEthProtocolHook((p), (netif))

#endif /* LEAP_DEVICE_FIRMWARE_LWIP_HOOKS_H_ */
