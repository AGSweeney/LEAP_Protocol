/** Network HAL — generic_networkhandler.c; NetBurner backend in opener_hal_netburner.cpp. */

#ifndef OPENER_NET_HAL_H_
#define OPENER_NET_HAL_H_

#include "opener_hal_types.h"
#include "ciptypes.h"
#include "cipstring.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Bind HAL to a network interface; store handle for subsequent socket calls. */
OpenerHalStatus OpenerHal_NetInit(OpenerNetIfHandle netif);

/** @brief Release interface-specific state (optional on embedded targets). */
void OpenerHal_NetShutdown(void);

/** @brief Listen for explicit messaging TCP on port (typically 44818). */
OpenerSocketHandle OpenerHal_TcpListen(CipUint port);

/** @brief Accept one incoming TCP connection; fill peer endpoint for encapsulation. */
OpenerSocketHandle OpenerHal_TcpAccept(OpenerSocketHandle listener,
                                       OpenerHalEndpoint *peer_out);

/** @brief Connect outbound TCP (originator use — rarely needed on adapters). */
OpenerHalStatus OpenerHal_TcpConnect(OpenerSocketHandle socket,
                                     const OpenerHalEndpoint *peer);

/** @brief Send on established TCP socket. */
OpenerHalStatus OpenerHal_TcpSend(OpenerSocketHandle socket,
                                  const CipOctet *data,
                                  size_t length);

/** @brief Receive from TCP socket; returns kOpenerHalWouldBlock when no data. */
OpenerHalStatus OpenerHal_TcpRecv(OpenerSocketHandle socket,
                                  CipOctet *buffer,
                                  size_t buffer_length,
                                  size_t *received_out);

/** UDP socket role — NetBurner shares one fd for explicit unicast and broadcast. */
typedef enum {
  kOpenerHalUdpExplicitUnicast = 0,
  kOpenerHalUdpExplicitBroadcast = 1,
  kOpenerHalUdpIoMessaging = 2
} OpenerHalUdpRole;

/** @brief Open UDP socket for List Identity, I/O (2222), or related ENIP UDP roles. */
OpenerSocketHandle OpenerHal_UdpOpen(CipUint port, OpenerHalUdpRole role);

/** @brief Send UDP datagram; records peer in internal table for OpenerHal_GetPeer(). */
OpenerHalStatus OpenerHal_UdpSend(OpenerSocketHandle socket,
                                  const OpenerHalEndpoint *dest,
                                  const CipOctet *data,
                                  size_t length);

/** @brief Receive UDP datagram and source endpoint. */
OpenerHalStatus OpenerHal_UdpRecv(OpenerSocketHandle socket,
                                  OpenerHalEndpoint *source_out,
                                  CipOctet *buffer,
                                  size_t buffer_length,
                                  size_t *received_out);

/** @brief Set O_NONBLOCK equivalent for socket polling loop. */
OpenerHalStatus OpenerHal_SocketSetNonBlocking(OpenerSocketHandle socket);

/** @brief Apply QoS DSCP (CIP QoS object); NetBurner port accepts but may not mark traffic yet. */
OpenerHalStatus OpenerHal_SocketSetQoS(OpenerSocketHandle socket,
                                       CipUsint qos_value);

/** @brief Close socket and clear peer table entry. */
void OpenerHal_SocketClose(OpenerSocketHandle socket);

/** @brief Shutdown without full close (TCP half-close path). */
void OpenerHal_SocketShutdown(OpenerSocketHandle socket);

/**
 * @brief Return last UDP/TCP peer for connected-style APIs.
 *
 * NetBurner UDP has no reliable getpeername; the HAL caches destination on send.
 */
OpenerHalStatus OpenerHal_GetPeer(OpenerSocketHandle socket,
                                  OpenerHalEndpoint *peer_out);

/** @brief Set IP multicast TTL (Class 1 I/O). NetBurner sets global bTTL_Default. */
OpenerHalStatus OpenerHal_SocketSetMulticastTtl(OpenerSocketHandle socket,
                                                CipUsint ttl);

/** @brief Bind multicast outbound interface (CIP multicast address selection). */
OpenerHalStatus OpenerHal_SocketSetMulticastIf(OpenerSocketHandle socket,
                                               CipUdint if_address);

/**
 * @brief Poll multiple read sockets (embedded select replacement).
 * @param timeout_ms 0 = non-blocking poll, -1 = block until one socket is readable.
 */
OpenerHalStatus OpenerHal_SocketPoll(OpenerSocketHandle *read_sockets,
                                     size_t read_count,
                                     int timeout_ms,
                                     OpenerSocketHandle *signaled_out,
                                     size_t *signaled_count_out);

/**
 * @brief Optional zero-copy UDP receive into NNDK-owned buffer.
 * @return kOpenerHalError when not implemented (fallback to OpenerHal_UdpRecv).
 */
OpenerHalStatus OpenerHal_UdpRecvZeroCopy(OpenerSocketHandle socket,
                                          OpenerHalEndpoint *source_out,
                                          OpenerHalBufferView *view_out);

/** @brief Release buffer view from OpenerHal_UdpRecvZeroCopy(). */
void OpenerHal_BufferRelease(OpenerHalBufferView *view);

/** @brief Last errno-style code from HAL network call. */
int OpenerHal_GetSocketError(void);

/** @brief Human-readable error string for logging. */
const char *OpenerHal_GetErrorString(int error_code);

/** @brief Read IPv4 address, mask, gateway, DNS into CIP TCP/IP object layout. */
OpenerHalStatus OpenerHal_GetInterfaceConfig(OpenerNetIfHandle netif,
                                             CipTcpIpInterfaceConfiguration *cfg_out);

/** @brief Read 6-byte MAC for Identity / Ethernet Link. */
OpenerHalStatus OpenerHal_GetMacAddress(OpenerNetIfHandle netif,
                                        uint8_t mac_out[6]);

/**
 * @brief Block until interface has non-zero IPv4 or timeout/abort.
 * @param abort_flag  Poll g_opener_abort; set non-zero to cancel wait during shutdown.
 */
OpenerHalStatus OpenerHal_WaitForIp(OpenerNetIfHandle netif,
                                    int timeout_sec,
                                    volatile int *abort_flag);

/** @brief Populate CipString hostname from NNDK interface name. */
void OpenerHal_GetHostName(OpenerNetIfHandle netif, CipString *hostname_out);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NET_HAL_H_ */
