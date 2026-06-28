/*******************************************************************************
 * Dev-only stub network HAL — use OPENER_NET_BACKEND=stub with
 * OPENER_BUILD_NETWORK_LAYER=OFF for host compile checks without NNDK.
 ******************************************************************************/

#include "opener_hal.h"

static char s_error_buffer[64] = "HAL not implemented";

OpenerHalStatus OpenerHal_NetInit(OpenerNetIfHandle netif) {
  (void)netif;
  return kOpenerHalError;
}

void OpenerHal_NetShutdown(void) {
}

OpenerSocketHandle OpenerHal_TcpListen(CipUint port) {
  (void)port;
  return OPENER_HAL_INVALID_SOCKET;
}

OpenerSocketHandle OpenerHal_TcpAccept(OpenerSocketHandle listener,
                                       OpenerHalEndpoint *peer_out) {
  (void)listener;
  (void)peer_out;
  return OPENER_HAL_INVALID_SOCKET;
}

OpenerHalStatus OpenerHal_TcpConnect(OpenerSocketHandle socket,
                                     const OpenerHalEndpoint *peer) {
  (void)socket;
  (void)peer;
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_TcpSend(OpenerSocketHandle socket,
                                  const CipOctet *data,
                                  size_t length) {
  (void)socket;
  (void)data;
  (void)length;
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_TcpRecv(OpenerSocketHandle socket,
                                  CipOctet *buffer,
                                  size_t buffer_length,
                                  size_t *received_out) {
  (void)socket;
  (void)buffer;
  (void)buffer_length;
  (void)received_out;
  return kOpenerHalError;
}

OpenerSocketHandle OpenerHal_UdpOpen(CipUint port, OpenerHalUdpRole role) {
  (void)port;
  (void)role;
  return OPENER_HAL_INVALID_SOCKET;
}

OpenerHalStatus OpenerHal_UdpSend(OpenerSocketHandle socket,
                                  const OpenerHalEndpoint *dest,
                                  const CipOctet *data,
                                  size_t length) {
  (void)socket;
  (void)dest;
  (void)data;
  (void)length;
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_UdpRecv(OpenerSocketHandle socket,
                                  OpenerHalEndpoint *source_out,
                                  CipOctet *buffer,
                                  size_t buffer_length,
                                  size_t *received_out) {
  (void)socket;
  (void)source_out;
  (void)buffer;
  (void)buffer_length;
  (void)received_out;
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_SocketSetNonBlocking(OpenerSocketHandle socket) {
  (void)socket;
  return kOpenerHalOk;
}

OpenerHalStatus OpenerHal_SocketSetQoS(OpenerSocketHandle socket,
                                       CipUsint qos_value) {
  (void)socket;
  (void)qos_value;
  return kOpenerHalOk;
}

void OpenerHal_SocketClose(OpenerSocketHandle socket) {
  (void)socket;
}

void OpenerHal_SocketShutdown(OpenerSocketHandle socket) {
  (void)socket;
}

OpenerHalStatus OpenerHal_GetPeer(OpenerSocketHandle socket,
                                  OpenerHalEndpoint *peer_out) {
  (void)socket;
  (void)peer_out;
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_SocketSetMulticastTtl(OpenerSocketHandle socket,
                                                CipUsint ttl) {
  (void)socket;
  (void)ttl;
  return kOpenerHalOk;
}

OpenerHalStatus OpenerHal_SocketSetMulticastIf(OpenerSocketHandle socket,
                                               CipUdint if_address) {
  (void)socket;
  (void)if_address;
  return kOpenerHalOk;
}

OpenerHalStatus OpenerHal_SocketPoll(OpenerSocketHandle *read_sockets,
                                     size_t read_count,
                                     int timeout_ms,
                                     OpenerSocketHandle *signaled_out,
                                     size_t *signaled_count_out) {
  (void)read_sockets;
  (void)read_count;
  (void)timeout_ms;
  (void)signaled_out;
  if(NULL != signaled_count_out) {
    *signaled_count_out = 0U;
  }
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_UdpRecvZeroCopy(OpenerSocketHandle socket,
                                          OpenerHalEndpoint *source_out,
                                          OpenerHalBufferView *view_out) {
  (void)socket;
  (void)source_out;
  (void)view_out;
  return kOpenerHalError;
}

void OpenerHal_BufferRelease(OpenerHalBufferView *view) {
  (void)view;
}

int OpenerHal_GetSocketError(void) {
  return 0;
}

const char *OpenerHal_GetErrorString(int error_code) {
  (void)error_code;
  return s_error_buffer;
}

OpenerHalStatus OpenerHal_GetInterfaceConfig(OpenerNetIfHandle netif,
                                             CipTcpIpInterfaceConfiguration *cfg_out) {
  (void)netif;
  (void)cfg_out;
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_GetMacAddress(OpenerNetIfHandle netif,
                                        uint8_t mac_out[6]) {
  (void)netif;
  (void)mac_out;
  return kOpenerHalError;
}

OpenerHalStatus OpenerHal_WaitForIp(OpenerNetIfHandle netif,
                                    int timeout_sec,
                                    volatile int *abort_flag) {
  (void)netif;
  (void)timeout_sec;
  (void)abort_flag;
  return kOpenerHalError;
}

void OpenerHal_GetHostName(OpenerNetIfHandle netif, CipString *hostname_out) {
  (void)netif;
  (void)hostname_out;
}

OpenerHalStatus OpenerHal_TimerInit(void) {
  return kOpenerHalOk;
}

OpenerHalTimestampUs OpenerHal_TimerGetMicroseconds(void) {
  return 0;
}
