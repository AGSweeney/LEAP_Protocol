/*******************************************************************************
 * NetBurner NNDK native network HAL for OpENer_uC-NetBurner.
 *
 * Maps OpenerHal_* to NNDK APIs:
 *   TCP listen/accept  — listenvia4, accept4
 *   UDP                — CreateRxTxUdpSocketVia4, sendto4, recvfrom4
 *   Poll               — select(nfds, &fds, NULL, NULL, ticks)
 *   Interface          — InterfaceIP/MAC via opener_nb_ifconfig.cpp
 *   Wait for IP        — WaitForActiveNetwork()
 *
 * NetBurner notes for integrators:
 *   - Interface numbers are 1-based (OpenerNetIfHandle carries ifnum).
 *   - Explicit UDP unicast and broadcast share one socket (s_explicit_udp_fd).
 *   - OpenerHal_GetPeer uses an internal peer table because UDP getpeername is unreliable.
 *   - OpenerHal_SocketSetMulticastTtl sets global bTTL_Default for Class 1 I/O.
 *   - Zero-copy UDP is not implemented; OpenerHal_UdpRecvZeroCopy returns kOpenerHalError.
 *
 * See opener_net_hal.h for the full HAL contract.
 ******************************************************************************/

#include <ip.h>
#include <predef.h>
#include <tcp.h>
#include <udp.h>
#include <iosys.h>
#include <netinterface.h>
#include <constants.h>
#include <init.h>
#include <errno.h>
#include <string.h>

#include "opener_hal.h"
#include "opener_nb_config.h"
#include "opener_nb_ifconfig.h"
extern "C" {
#include "cipstring.h"
}

#ifndef OPENER_NB_PEER_TABLE_SIZE
#define OPENER_NB_PEER_TABLE_SIZE 32
#endif

extern "C" int select(int nfds,
                      fd_set *readfds,
                      fd_set *writefds,
                      fd_set *errorfds,
                      unsigned long timeout);

namespace {

struct NbPeerInfo {
  int in_use;
  int fd;
  uint32_t address;
  uint16_t port;
};

static NbPeerInfo g_peer_table[OPENER_NB_PEER_TABLE_SIZE];
static int s_ifnum = OPENER_NB_DEFAULT_IFNUM;
static int s_explicit_udp_fd = OPENER_HAL_INVALID_SOCKET;
static int s_io_udp_fd = OPENER_HAL_INVALID_SOCKET;
static int s_last_error = 0;

int NetifToIfnum(OpenerNetIfHandle netif) {
  if(NULL == netif) {
    return s_ifnum;
  }
  const intptr_t value = (intptr_t)netif;
  if(value <= 0) {
    return s_ifnum;
  }
  return (int)value;
}

void SetLastError(int error_code) {
  s_last_error = error_code;
  errno = error_code;
}

NbPeerInfo *FindPeerSlotForFd(int fd) {
  if(fd < 0) {
    return NULL;
  }
  for(size_t i = 0U; i < OPENER_NB_PEER_TABLE_SIZE; ++i) {
    if((g_peer_table[i].in_use != 0) && (g_peer_table[i].fd == fd)) {
      return &g_peer_table[i];
    }
  }
  return NULL;
}

NbPeerInfo *ReservePeerSlotForFd(int fd) {
  NbPeerInfo *slot = FindPeerSlotForFd(fd);
  if(NULL != slot) {
    return slot;
  }
  for(size_t i = 0U; i < OPENER_NB_PEER_TABLE_SIZE; ++i) {
    if(g_peer_table[i].in_use == 0) {
      return &g_peer_table[i];
    }
  }
  return NULL;
}

void StorePeer(int fd, uint32_t address, uint16_t port) {
  NbPeerInfo *slot = ReservePeerSlotForFd(fd);
  if(NULL != slot) {
    slot->in_use = 1;
    slot->fd = fd;
    slot->address = OpenerNbIpv4ToCip(address);
    slot->port = port;
  }
}

void ClearPeerSlotForFd(int fd) {
  NbPeerInfo *slot = FindPeerSlotForFd(fd);
  if(NULL != slot) {
    memset(slot, 0, sizeof(*slot));
  }
}

IPADDR4 IpAddr4FromEndpoint(const OpenerHalEndpoint *endpoint) {
  if((NULL == endpoint) || (0U == endpoint->address)) {
    return IPADDR4::NullIP();
  }
  return IPADDR4(OpenerNbCipToIpv4(endpoint->address));
}

void EndpointFromIpPort(uint32_t address, uint16_t port, OpenerHalEndpoint *endpoint_out) {
  if(NULL != endpoint_out) {
    endpoint_out->address = address;
    endpoint_out->port = port;
  }
}

unsigned long TimeoutMsToTicks(int timeout_ms) {
  if(timeout_ms < 0) {
    return 0UL;
  }
  if(0 == timeout_ms) {
    return 0UL;
  }
  unsigned long ticks =
    ((unsigned long)timeout_ms * (unsigned long)TICKS_PER_SECOND) / 1000UL;
  if(0UL == ticks) {
    ticks = 1UL;
  }
  return ticks;
}

} /* namespace */

extern "C" int OpenerNbNetifToIfnum(OpenerNetIfHandle netif) {
  return NetifToIfnum(netif);
}

extern "C" OpenerHalStatus OpenerHal_NetInit(OpenerNetIfHandle netif) {
  s_ifnum = NetifToIfnum(netif);
  if(s_ifnum <= 0) {
    s_ifnum = OPENER_NB_DEFAULT_IFNUM;
  }
  return kOpenerHalOk;
}

extern "C" void OpenerHal_NetShutdown(void) {
  s_explicit_udp_fd = OPENER_HAL_INVALID_SOCKET;
  s_io_udp_fd = OPENER_HAL_INVALID_SOCKET;
  memset(g_peer_table, 0, sizeof(g_peer_table));
}

extern "C" OpenerSocketHandle OpenerHal_TcpListen(CipUint port) {
  const uint8_t max_pending = 5U;
  const IPADDR4 iface_ip = InterfaceIP(s_ifnum);

  if(iface_ip.IsNull()) {
    SetLastError(EIO);
    return OPENER_HAL_INVALID_SOCKET;
  }

  const int fd = listenvia4(IPADDR4::NullIP(), port, iface_ip, max_pending);
  if(fd < 0) {
    SetLastError(EIO);
    return OPENER_HAL_INVALID_SOCKET;
  }
  return fd;
}

extern "C" OpenerSocketHandle OpenerHal_TcpAccept(OpenerSocketHandle listener,
                                                   OpenerHalEndpoint *peer_out) {
  IPADDR4 peer_ip;
  uint16_t peer_port = 0U;
  const int client_fd = accept4(listener, &peer_ip, &peer_port, (uint16_t)0);

  if(client_fd < 0) {
    if(client_fd == TCP_ERR_TIMEOUT) {
      SetLastError(EWOULDBLOCK);
    } else {
      SetLastError(EIO);
    }
    return OPENER_HAL_INVALID_SOCKET;
  }

  const uint32_t peer_address = (uint32_t)peer_ip;
  StorePeer(client_fd, peer_address, peer_port);
  EndpointFromIpPort(OpenerNbIpv4ToCip(peer_address), peer_port, peer_out);
  return client_fd;
}

extern "C" OpenerHalStatus OpenerHal_TcpConnect(OpenerSocketHandle socket,
                                                const OpenerHalEndpoint *peer) {
  (void)socket;
  (void)peer;
  SetLastError(EOPNOTSUPP);
  return kOpenerHalError;
}

extern "C" OpenerHalStatus OpenerHal_TcpSend(OpenerSocketHandle socket,
                                             const CipOctet *data,
                                             size_t length) {
  if((NULL == data) || (0U == length)) {
    return kOpenerHalError;
  }

  const int sent = write(socket, (const char *)data, (int)length);
  if(sent < 0) {
    SetLastError(EIO);
    return kOpenerHalError;
  }
  if((size_t)sent != length) {
    SetLastError(EIO);
    return kOpenerHalError;
  }
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_TcpRecv(OpenerSocketHandle socket,
                                             CipOctet *buffer,
                                             size_t buffer_length,
                                             size_t *received_out) {
  if((NULL == buffer) || (NULL == received_out)) {
    return kOpenerHalError;
  }

  const int received = read(socket, (char *)buffer, (int)buffer_length);
  if(received < 0) {
    if(received == TCP_ERR_TIMEOUT) {
      SetLastError(EWOULDBLOCK);
      *received_out = 0U;
      return kOpenerHalWouldBlock;
    }
    SetLastError(EIO);
    *received_out = 0U;
    return kOpenerHalError;
  }
  if(0 == received) {
    *received_out = 0U;
    return kOpenerHalError;
  }

  *received_out = (size_t)received;
  return kOpenerHalOk;
}

extern "C" OpenerSocketHandle OpenerHal_UdpOpen(CipUint port, OpenerHalUdpRole role) {
  if((role == kOpenerHalUdpExplicitUnicast) ||
     (role == kOpenerHalUdpExplicitBroadcast)) {
    if(s_explicit_udp_fd >= 0) {
      return s_explicit_udp_fd;
    }
    const int fd =
      CreateRxTxUdpSocketVia4(IPADDR4::NullIP(), 0, port, s_ifnum);
    if(fd < 0) {
      SetLastError(EIO);
      return OPENER_HAL_INVALID_SOCKET;
    }
    s_explicit_udp_fd = fd;
    return fd;
  }

  if(role == kOpenerHalUdpIoMessaging) {
    if(s_io_udp_fd >= 0) {
      return s_io_udp_fd;
    }
    const int fd =
      CreateRxTxUdpSocketVia4(IPADDR4::NullIP(), 0, port, s_ifnum);
    if(fd < 0) {
      SetLastError(EIO);
      return OPENER_HAL_INVALID_SOCKET;
    }
    s_io_udp_fd = fd;
    return fd;
  }

  SetLastError(EINVAL);
  return OPENER_HAL_INVALID_SOCKET;
}

extern "C" OpenerHalStatus OpenerHal_UdpSend(OpenerSocketHandle socket,
                                             const OpenerHalEndpoint *dest,
                                             const CipOctet *data,
                                             size_t length) {
  if((NULL == dest) || (NULL == data) || (0U == length)) {
    return kOpenerHalError;
  }

  const IPADDR4 dest_ip = IpAddr4FromEndpoint(dest);
  const int sent = sendto4(socket,
                           (puint8_t)data,
                           (int)length,
                           dest_ip,
                           dest->port);
  if(sent < 0) {
    SetLastError(EIO);
    return kOpenerHalError;
  }
  if((size_t)sent != length) {
    SetLastError(EIO);
    return kOpenerHalError;
  }
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_UdpRecv(OpenerSocketHandle socket,
                                             OpenerHalEndpoint *source_out,
                                             CipOctet *buffer,
                                             size_t buffer_length,
                                             size_t *received_out) {
  IPADDR4 src_ip = IPADDR4::NullIP();
  uint16_t local_port = 0U;
  uint16_t remote_port = 0U;

  if((NULL == buffer) || (NULL == received_out)) {
    return kOpenerHalError;
  }

  const int received = recvfrom4(socket,
                                 (puint8_t)buffer,
                                 (int)buffer_length,
                                 &src_ip,
                                 &local_port,
                                 &remote_port);
  if(received < 0) {
    SetLastError(EWOULDBLOCK);
    *received_out = 0U;
    return kOpenerHalWouldBlock;
  }

  const uint32_t src_address = (uint32_t)src_ip;
  StorePeer(socket, src_address, remote_port);
  EndpointFromIpPort(OpenerNbIpv4ToCip(src_address), remote_port, source_out);
  *received_out = (size_t)received;
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_SocketSetNonBlocking(OpenerSocketHandle socket) {
  (void)socket;
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_SocketSetQoS(OpenerSocketHandle socket,
                                                  CipUsint qos_value) {
  (void)socket;
  (void)qos_value;
  return kOpenerHalOk;
}

extern "C" void OpenerHal_SocketClose(OpenerSocketHandle socket) {
  ClearPeerSlotForFd(socket);
  if(socket == s_explicit_udp_fd) {
    s_explicit_udp_fd = OPENER_HAL_INVALID_SOCKET;
  }
  if(socket == s_io_udp_fd) {
    s_io_udp_fd = OPENER_HAL_INVALID_SOCKET;
  }
  close(socket);
}

extern "C" void OpenerHal_SocketShutdown(OpenerSocketHandle socket) {
  ClearPeerSlotForFd(socket);
}

extern "C" OpenerHalStatus OpenerHal_GetPeer(OpenerSocketHandle socket,
                                             OpenerHalEndpoint *peer_out) {
  const NbPeerInfo *slot = FindPeerSlotForFd(socket);
  if((NULL == slot) || (slot->in_use == 0) || (NULL == peer_out)) {
    SetLastError(ENOTCONN);
    return kOpenerHalError;
  }
  EndpointFromIpPort(slot->address, slot->port, peer_out);
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_SocketSetMulticastTtl(OpenerSocketHandle socket,
                                                           CipUsint ttl) {
  (void)socket;
  bTTL_Default = ttl;
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_SocketSetMulticastIf(OpenerSocketHandle socket,
                                                          CipUdint if_address) {
  (void)socket;
  (void)if_address;
  /* I/O UDP sockets are already bound to s_ifnum via CreateRxTxUdpSocketVia4. */
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_SocketPoll(OpenerSocketHandle *read_sockets,
                                                size_t read_count,
                                                int timeout_ms,
                                                OpenerSocketHandle *signaled_out,
                                                size_t *signaled_count_out) {
  fd_set read_fds;
  int nfds = 0;

  if((NULL == read_sockets) || (NULL == signaled_out) ||
     (NULL == signaled_count_out)) {
    return kOpenerHalError;
  }

  FD_ZERO(&read_fds);
  for(size_t i = 0U; i < read_count; ++i) {
    const OpenerSocketHandle socket = read_sockets[i];
    if(socket >= 0) {
      FD_SET(socket, &read_fds);
      if(socket >= nfds) {
        nfds = socket + 1;
      }
    }
  }

  const int ready = select(nfds, &read_fds, NULL, NULL, TimeoutMsToTicks(timeout_ms));
  *signaled_count_out = 0U;

  if(ready < 0) {
    SetLastError(EIO);
    return kOpenerHalError;
  }
  if(0 == ready) {
    return kOpenerHalTimeout;
  }

  for(size_t i = 0U; i < read_count; ++i) {
    const OpenerSocketHandle socket = read_sockets[i];
    if((socket >= 0) && FD_ISSET(socket, &read_fds)) {
      signaled_out[*signaled_count_out] = socket;
      ++(*signaled_count_out);
    }
  }
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_UdpRecvZeroCopy(OpenerSocketHandle socket,
                                                     OpenerHalEndpoint *source_out,
                                                     OpenerHalBufferView *view_out) {
  (void)socket;
  (void)source_out;
  (void)view_out;
  return kOpenerHalError;
}

extern "C" void OpenerHal_BufferRelease(OpenerHalBufferView *view) {
  (void)view;
}

extern "C" int OpenerHal_GetSocketError(void) {
  return s_last_error;
}

extern "C" const char *OpenerHal_GetErrorString(int error_code) {
  switch(error_code) {
    case EWOULDBLOCK:
      return "would block";
    case ENOTCONN:
      return "not connected";
    case EIO:
      return "I/O error";
    case EOPNOTSUPP:
      return "operation not supported";
    default:
      return "network error";
  }
}

extern "C" OpenerHalStatus OpenerHal_GetInterfaceConfig(OpenerNetIfHandle netif,
                                                        CipTcpIpInterfaceConfiguration *cfg_out) {
  OpenerNbIpv4Config ipv4_cfg;
  const int ifnum = NetifToIfnum(netif);

  if((NULL == cfg_out) || (OpenerNbIfaceGetIpv4Config(ifnum, &ipv4_cfg) != 0)) {
    return kOpenerHalError;
  }

  cfg_out->ip_address = ipv4_cfg.ip;
  cfg_out->network_mask = ipv4_cfg.mask;
  cfg_out->gateway = ipv4_cfg.gateway;
  cfg_out->name_server = ipv4_cfg.dns1;
  cfg_out->name_server_2 = ipv4_cfg.dns2;
  return kOpenerHalOk;
}

extern "C" OpenerHalStatus OpenerHal_GetMacAddress(OpenerNetIfHandle netif,
                                                   uint8_t mac_out[6]) {
  const int ifnum = NetifToIfnum(netif);
  return (OpenerNbIfaceGetMac(ifnum, mac_out) == 0) ? kOpenerHalOk : kOpenerHalError;
}

extern "C" OpenerHalStatus OpenerHal_WaitForIp(OpenerNetIfHandle netif,
                                               int timeout_sec,
                                               volatile int *abort_flag) {
  const int ifnum = NetifToIfnum(netif);
  unsigned long ticks = (timeout_sec > 0) ?
                        ((unsigned long)timeout_sec * (unsigned long)TICKS_PER_SECOND) :
                        (120UL * (unsigned long)TICKS_PER_SECOND);

  if((NULL != abort_flag) && (0 != *abort_flag)) {
    return kOpenerHalError;
  }

  if(WaitForActiveNetwork(ticks, ifnum)) {
    return kOpenerHalOk;
  }
  return kOpenerHalTimeout;
}

extern "C" void OpenerHal_GetHostName(OpenerNetIfHandle netif, CipString *hostname_out) {
  char name_buf[64];
  const int ifnum = OpenerNbNetifToIfnum(netif);

  if((NULL != hostname_out) &&
     (0 == OpenerNbIfaceGetHostName(ifnum, name_buf, sizeof(name_buf)))) {
    SetCipStringByCstr(hostname_out, name_buf);
  } else if(NULL != hostname_out) {
    SetCipStringByCstr(hostname_out, "NetBurner");
  }
}
