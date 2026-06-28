#ifndef OPENER_NB_ACD_CIP_H_
#define OPENER_NB_ACD_CIP_H_

/*******************************************************************************
 * OpENer_uC-NetBurner — TCP/IP object attributes 10/11 bridge for wire ACD
 *
 * Registered from opener_nb_acd_cip.c when OPENER_NB_ACD is enabled. Keeps
 * g_tcpip.status capability/conflict/fault bits aligned with OpenerNbAcd* state.
 ******************************************************************************/

#include "opener_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Refresh g_tcpip ACD capability and status bits from wire module (each process tick). */
void OpenerNbAcdApplyTcpIpObject(void);

/** @brief Encode TCP/IP attribute 11 (Last Conflict) for GetAttribute. */
void OpenerNbAcdEncodeLastConflict(const void *const data,
                                   ENIPMessage *const outgoing_message);

/** @brief Decode SetAttribute on TCP/IP attribute 10 (Select ACD). */
int OpenerNbAcdDecodeSelectAcd(void *const data,
                               CipMessageRouterRequest *const message_router_request,
                               CipMessageRouterResponse *const message_router_response);

/** @brief Decode SetAttribute on TCP/IP attribute 11 (clear conflict — 35 zero bytes). */
int OpenerNbAcdDecodeClearLastConflict(
  void *const data,
  CipMessageRouterRequest *const message_router_request,
  CipMessageRouterResponse *const message_router_response);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_ACD_CIP_H_ */
