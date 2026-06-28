/** NNDK glue for TCP/IP and Ethernet Link — called from opener_init() before CipStackInit. */
#ifndef OPENER_NB_APP_NETWORKCONFIG_H_
#define OPENER_NB_APP_NETWORKCONFIG_H_

#include "opener_app_preinclude.h"
#include "opener_api.h"
#include "opener_hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

EipStatus OpenerNbPrepareNetworkStack(OpenerNetIfHandle netif, int timeout_sec);

EipStatus IfaceGetConfiguration(TcpIpInterface iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg);

void GetHostName(TcpIpInterface iface, CipString *hostname);

EipStatus OpenerNbApplyTcpIpConfiguration(TcpIpInterface iface);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_APP_NETWORKCONFIG_H_ */
