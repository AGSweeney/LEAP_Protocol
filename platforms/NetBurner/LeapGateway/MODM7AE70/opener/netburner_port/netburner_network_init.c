/*******************************************************************************
 * OpENer network bootstrap using NetBurner socket helpers.
 ******************************************************************************/

#include "generic_networkhandler.h"
#include "cipqos.h"
#include "cipconnectionobject.h"
#include "networkhandler.h"
#include "trace.h"

#include <stdint.h>
#include <unistd.h>

extern int g_opener_plant_ifnum;
extern int nb_tcp_listen(uint16_t port, int backlog);
extern int nb_udp_listen(uint16_t port);
extern int nb_udp_io_socket(uint16_t port);

#define MAX_NO_OF_TCP_SOCKETS 10

EipStatus NetburnerOpenerNetworkInit(void) {
  const uint32_t device_ip = g_network_status.ip_address;
  const uint16_t cip_port = kOpenerEthernetPort;

  if (device_ip == 0U) {
    OPENER_TRACE_ERR("NetburnerOpenerNetworkInit: plant interface has no IP\n");
    return kEipStatusError;
  }

  g_network_status.tcp_listener = nb_tcp_listen(cip_port, MAX_NO_OF_TCP_SOCKETS);
  if (g_network_status.tcp_listener < 0) {
    OPENER_TRACE_ERR("NetburnerOpenerNetworkInit: TCP listen failed\n");
    return kEipStatusError;
  }

  if (SetSocketToNonBlocking(g_network_status.tcp_listener) < 0) {
    OPENER_TRACE_ERR("NetburnerOpenerNetworkInit: TCP non-blocking failed\n");
    return kEipStatusError;
  }

  g_network_status.udp_unicast_listener = nb_udp_listen(cip_port);
  if (g_network_status.udp_unicast_listener < 0) {
    OPENER_TRACE_ERR("NetburnerOpenerNetworkInit: UDP listen failed\n");
    return kEipStatusError;
  }

  g_network_status.udp_global_broadcast_listener = g_network_status.udp_unicast_listener;

  if (SetSocketToNonBlocking(g_network_status.udp_unicast_listener) < 0) {
    OPENER_TRACE_ERR("NetburnerOpenerNetworkInit: UDP non-blocking failed\n");
    return kEipStatusError;
  }

  (void)SetQosOnSocket(g_network_status.udp_unicast_listener,
                       CipQosGetDscpPriority(kConnectionObjectPriorityExplicit));
  (void)SetQosOnSocket(g_network_status.tcp_listener,
                       CipQosGetDscpPriority(kConnectionObjectPriorityExplicit));

  FD_SET(g_network_status.tcp_listener, &master_socket);
  FD_SET(g_network_status.udp_unicast_listener, &master_socket);
  FD_SET(g_network_status.udp_global_broadcast_listener, &master_socket);

  highest_socket_handle = GetMaxSocket(g_network_status.tcp_listener,
                                       g_network_status.udp_global_broadcast_listener,
                                       0,
                                       g_network_status.udp_unicast_listener);
  return kEipStatusOk;
}

int NetburnerCreateUdpIoSocket(void) {
  const int fd = nb_udp_io_socket(kOpenerEipIoUdpPort);
  if (fd < 0) {
    return kEipInvalidSocket;
  }

  if (SetSocketToNonBlocking(fd) < 0) {
    close(fd);
    return kEipInvalidSocket;
  }

  FD_SET(fd, &master_socket);
  if (fd > highest_socket_handle) {
    highest_socket_handle = fd;
  }
  return fd;
}
