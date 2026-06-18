/*******************************************************************************
 * TCP/IP object NV hooks - delegates to NetBurner InterfaceBlock flash storage.
 ******************************************************************************/

#include "nvtcpip.h"

#include "nb_nvtcpip.h"
#include "trace.h"

static void TcpipToNvConfig(const CipTcpIpObject *p_tcp_ip, NbTcpIpNvConfig *cfg_out) {
  cfg_out->config_control = p_tcp_ip->config_control;
  cfg_out->ip = p_tcp_ip->interface_configuration.ip_address;
  cfg_out->mask = p_tcp_ip->interface_configuration.network_mask;
  cfg_out->gateway = p_tcp_ip->interface_configuration.gateway;
  cfg_out->dns1 = p_tcp_ip->interface_configuration.name_server;
  cfg_out->dns2 = p_tcp_ip->interface_configuration.name_server_2;
}

static void NvConfigToTcpip(const NbTcpIpNvConfig *cfg_in, CipTcpIpObject *p_tcp_ip) {
  p_tcp_ip->config_control =
    (p_tcp_ip->config_control & ~kTcpipCfgCtrlMethodMask) |
    (cfg_in->config_control & kTcpipCfgCtrlMethodMask);
  p_tcp_ip->interface_configuration.ip_address = cfg_in->ip;
  p_tcp_ip->interface_configuration.network_mask = cfg_in->mask;
  p_tcp_ip->interface_configuration.gateway = cfg_in->gateway;
  p_tcp_ip->interface_configuration.name_server = cfg_in->dns1;
  p_tcp_ip->interface_configuration.name_server_2 = cfg_in->dns2;
}

EipStatus NvTcpipLoad(CipTcpIpObject *p_tcp_ip) {
  NbTcpIpNvConfig cfg;

  if (p_tcp_ip == NULL) {
    return kEipStatusError;
  }

  if (nb_nv_tcpip_load(&cfg) != 0) {
    OPENER_TRACE_INFO("NvTcpipLoad: using TCP/IP defaults\n");
    return kEipStatusError;
  }

  NvConfigToTcpip(&cfg, p_tcp_ip);
  return kEipStatusOk;
}

EipStatus NvTcpipStore(const CipTcpIpObject *p_tcp_ip) {
  NbTcpIpNvConfig cfg;

  if (p_tcp_ip == NULL) {
    return kEipStatusError;
  }

  TcpipToNvConfig(p_tcp_ip, &cfg);
  if (nb_nv_tcpip_store(&cfg) != 0) {
    OPENER_TRACE_ERR("NvTcpipStore: failed to save TCP/IP configuration\n");
    return kEipStatusError;
  }

  return kEipStatusOk;
}
