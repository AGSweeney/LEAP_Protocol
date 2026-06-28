/*******************************************************************************
 * OpENer_uC-NetBurner — TCP/IP object attributes 10/11 bridge for wire ACD
 *
 * Wired from ciptcpipinterface.c when OPENER_NB_ACD is enabled at build time.
 * OpenerNbAcdApplyTcpIpObject() is also called each opener_process() tick from opener.c.
 ******************************************************************************/

#include "opener_nb_acd_cip.h"

#if OPENER_NB_ACD

#include "opener_nb_acd.h"
#include "opener_nb_platform_types.h"

#include "cipcommon.h"
#include "ciperror.h"
#include "ciptcpipinterface.h"
#include "endianconv.h"
#include "trace.h"

#include <string.h>

void OpenerNbAcdApplyTcpIpObject(void) {
  g_tcpip.config_capability |= kTcpipCfgCapsAcdCapable;
  g_tcpip.select_acd = OpenerNbAcdGetSelectAcd();

  g_tcpip.status &= (CipDword)(~(kTcpipStatusAcdStatus | kTcpipStatusAcdFault));
  if(OpenerNbAcdStatusConflict()) {
    g_tcpip.status |= kTcpipStatusAcdStatus;
  }
  if(OpenerNbAcdStatusFault()) {
    g_tcpip.status |= kTcpipStatusAcdFault;
  }
}

void OpenerNbAcdEncodeLastConflict(const void *const data,
                                   ENIPMessage *const outgoing_message) {
  OpenerNbAcdLastConflict conflict;
  (void)data;

  OpenerNbAcdGetLastConflict(&conflict);
  EncodeCipUsint(&conflict.acd_activity, outgoing_message);
  for(int i = 0; i < 6; ++i) {
    EncodeCipUsint(&conflict.remote_mac[i], outgoing_message);
  }
  for(int i = 0; i < 28; ++i) {
    EncodeCipUsint(&conflict.arp_pdu[i], outgoing_message);
  }
}

int OpenerNbAcdDecodeSelectAcd(void *const data,
                               CipMessageRouterRequest *const message_router_request,
                               CipMessageRouterResponse *const message_router_response) {
  bool needs_reset = false;
  opener_nb_status_t status = OPENER_NB_ERR_INVALID_ARG;

  if((NULL == data) || (NULL == message_router_request) ||
     (NULL == message_router_response)) {
    return -1;
  }

  if(message_router_request->request_data_size < 1U) {
    message_router_response->general_status = kCipErrorNotEnoughData;
    return -1;
  }

  status = OpenerNbAcdSetSelectAcd(message_router_request->data[0] != 0U, &needs_reset);
  if(OPENER_NB_OK != status) {
    message_router_response->general_status = kCipErrorAttributeNotSetable;
    return -1;
  }

  *(CipBool *)data = OpenerNbAcdGetSelectAcd();
  g_tcpip.select_acd = *(CipBool *)data;
  message_router_response->general_status = kCipErrorSuccess;
  (void)needs_reset;
  return 1;
}

int OpenerNbAcdDecodeClearLastConflict(
  void *const data,
  CipMessageRouterRequest *const message_router_request,
  CipMessageRouterResponse *const message_router_response) {
  size_t i = 0U;

  (void)data;
  if((NULL == message_router_request) || (NULL == message_router_response)) {
    return -1;
  }

  if(message_router_request->request_data_size != 35U) {
    message_router_response->general_status = kCipErrorInvalidAttributeValue;
    return -1;
  }

  for(i = 0U; i < message_router_request->request_data_size; ++i) {
    if(0U != message_router_request->data[i]) {
      message_router_response->general_status = kCipErrorInvalidAttributeValue;
      return -1;
    }
  }

  if(OPENER_NB_OK != OpenerNbAcdClearLastConflict()) {
    message_router_response->general_status = kCipErrorAttributeNotSetable;
    return -1;
  }

  OpenerNbAcdApplyTcpIpObject();
  message_router_response->general_status = kCipErrorSuccess;
  return (int)message_router_request->request_data_size;
}

#endif /* OPENER_NB_ACD */
