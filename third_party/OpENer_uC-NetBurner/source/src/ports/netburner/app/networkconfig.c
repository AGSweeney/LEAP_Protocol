/*******************************************************************************
 * OpENer_uC-NetBurner — NNDK to CIP network object synchronization
 *
 * See networkconfig.h for public API. Identity serial number is derived from
 * MAC bytes 2–5 (same scheme as many adapter devices).
 ******************************************************************************/

#include "networkconfig.h"

#include <string.h>

#include "cipethernetlink.h"
#include "cipidentity.h"
#include "cipstring.h"
#include "ciptcpipinterface.h"
#include "opener_mem_hal.h"
#include "opener_net_hal.h"
#include "opener_nb_acd.h"
#include "opener_nb_config.h"
#include "opener_nb_ifconfig.h"
#if OPENER_NB_LLDP
#include "opener_nb_identity.h"
#include "opener_nb_lldp.h"
#endif
#include "trace.h"

extern volatile int g_opener_abort;

/** Derive 32-bit Identity serial from lower four MAC bytes. */
static CipUdint SerialNumberFromMac(const uint8_t *mac) {
  const CipUdint serial = ((CipUdint)mac[2] << 24) |
                          ((CipUdint)mac[3] << 16) |
                          ((CipUdint)mac[4] << 8) |
                          (CipUdint)mac[5];
  return serial & 0x7FFFFFFFUL;
}

/** Map NNDK DHCP/static mode to g_tcpip.config_control and Identity configured flag. */
static void OpenerNbSyncTcpIpObjectFromNndk(OpenerNetIfHandle netif) {
  const int ifnum = OpenerNbNetifToIfnum(netif);
  const OpenerNbIpConfigMethod method = OpenerNbIfaceGetIpConfigMethod(ifnum);

  g_tcpip.status = (g_tcpip.status & (kTcpipStatusMcastPend |
                                      kTcpipStatusIfaceCfgPend |
                                      kTcpipStatusAcdStatus |
                                      kTcpipStatusAcdFault)) | 0x01U;

  switch(method) {
    case kOpenerNbIpConfigStatic:
      g_tcpip.config_control = kTcpipCfgCtrlStaticIp;
      CipIdentitySetStatusFlags(kConfigured);
      break;
    case kOpenerNbIpConfigDhcp:
      g_tcpip.config_control = kTcpipCfgCtrlDhcp;
      break;
    default:
      OPENER_TRACE_WARN("OpenerNbSyncTcpIpObjectFromNndk: unknown IP mode on if %d\n",
                        ifnum);
      g_tcpip.config_control = kTcpipCfgCtrlDhcp;
      break;
  }
}

static OpenerNbIpConfigMethod OpenerNbMethodFromConfigControl(CipDword config_control) {
  switch(config_control & kTcpipCfgCtrlMethodMask) {
    case kTcpipCfgCtrlStaticIp:
      return kOpenerNbIpConfigStatic;
    case kTcpipCfgCtrlBootp:
    case kTcpipCfgCtrlDhcp:
      return kOpenerNbIpConfigDhcp;
    default:
      return kOpenerNbIpConfigUnknown;
  }
}

#if OPENER_NB_LLDP
/** Refresh LLDP chassis/port/CIP TLV sources after TCP/IP or hostname change. */
static void OpenerNbRefreshLldpIdentity(void) {
  OpenerNbLldpIdentity identity;

  OpenerNbLldpIdentityFromGlobals(&identity);
  OpenerNbLldpUpdateIdentity(&identity);
}
#endif

EipStatus OpenerNbPrepareNetworkStack(OpenerNetIfHandle netif, int timeout_sec) {
  uint8_t mac[6];
  const int ifnum = OpenerNbNetifToIfnum(netif);

  if(kOpenerHalOk != OpenerHal_MemInit()) {
    return kEipStatusError;
  }

  if(kOpenerHalOk != OpenerHal_NetInit(netif)) {
    return kEipStatusError;
  }

  if(kOpenerHalOk != OpenerHal_WaitForIp(netif, timeout_sec, &g_opener_abort)) {
    OPENER_TRACE_ERR("OpenerNbPrepareNetworkStack: WaitForIp failed\n");
    return kEipStatusError;
  }

  if(kOpenerHalOk != OpenerHal_GetMacAddress(netif, mac)) {
    return kEipStatusError;
  }
  CipEthernetLinkSetMac(mac);
  SetDeviceSerialNumber(SerialNumberFromMac(mac));

  if(kOpenerHalOk != OpenerHal_GetInterfaceConfig(netif,
                                                   &g_tcpip.interface_configuration)) {
    return kEipStatusError;
  }

  OpenerHal_GetHostName(netif, &g_tcpip.hostname);
  CipTcpIpCalculateMulticastIp(&g_tcpip);
  OpenerNbSyncTcpIpObjectFromNndk(netif);
  OpenerNbSyncEthernetLinkFromNndk(ifnum);

#if OPENER_NB_ACD
  if(kOpenerNbIpConfigDhcp == OpenerNbIfaceGetIpConfigMethod(ifnum)) {
    OpenerNbAcdNotifyDhcpBound(ifnum);
  }
#endif

  return kEipStatusOk;
}

EipStatus IfaceGetConfiguration(TcpIpInterface iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg) {
  EipStatus status = kEipStatusError;

  if(NULL == iface_cfg) {
    return kEipStatusError;
  }

  if(OpenerHal_GetInterfaceConfig(iface, iface_cfg) != kOpenerHalOk) {
    return kEipStatusError;
  }

  g_tcpip.interface_configuration = *iface_cfg;
  CipTcpIpCalculateMulticastIp(&g_tcpip);
#if OPENER_NB_LLDP
  OpenerNbRefreshLldpIdentity();
#endif
#if OPENER_NB_ACD
  if(kOpenerNbIpConfigDhcp ==
     OpenerNbIfaceGetIpConfigMethod(OpenerNbNetifToIfnum(iface))) {
    OpenerNbAcdNotifyDhcpBound(OpenerNbNetifToIfnum(iface));
  }
#endif
  status = kEipStatusOk;
  return status;
}

void GetHostName(TcpIpInterface iface, CipString *hostname) {
  OpenerHal_GetHostName(iface, hostname);
#if OPENER_NB_LLDP
  OpenerNbRefreshLldpIdentity();
#endif
}

EipStatus OpenerNbApplyTcpIpConfiguration(TcpIpInterface iface) {
  const int ifnum = OpenerNbNetifToIfnum(iface);
  const OpenerNbIpConfigMethod method =
    OpenerNbMethodFromConfigControl(g_tcpip.config_control);
  OpenerNbIpv4Config ipv4_cfg;
  char host_name_buf[65];

  ipv4_cfg.ip = g_tcpip.interface_configuration.ip_address;
  ipv4_cfg.mask = g_tcpip.interface_configuration.network_mask;
  ipv4_cfg.gateway = g_tcpip.interface_configuration.gateway;
  ipv4_cfg.dns1 = g_tcpip.interface_configuration.name_server;
  ipv4_cfg.dns2 = g_tcpip.interface_configuration.name_server_2;

  if((ifnum <= 0) || (kOpenerNbIpConfigUnknown == method)) {
    OPENER_TRACE_ERR("OpenerNbApplyTcpIpConfiguration: invalid iface/method\n");
    return kEipStatusError;
  }

  if(0 != OpenerNbIfaceSetIpConfigMethod(ifnum, method)) {
    OPENER_TRACE_ERR("OpenerNbApplyTcpIpConfiguration: failed to set IP mode\n");
    return kEipStatusError;
  }

  if((kOpenerNbIpConfigStatic == method) &&
     (0 != OpenerNbIfaceSetIpv4Config(ifnum, &ipv4_cfg))) {
    OPENER_TRACE_ERR("OpenerNbApplyTcpIpConfiguration: failed to set static IPv4\n");
    return kEipStatusError;
  }

  if((NULL != g_tcpip.hostname.string) && (g_tcpip.hostname.length > 0U)) {
    size_t copy_len = (size_t)g_tcpip.hostname.length;
    if(copy_len >= sizeof(host_name_buf)) {
      copy_len = sizeof(host_name_buf) - 1U;
    }
    memcpy(host_name_buf, g_tcpip.hostname.string, copy_len);
    host_name_buf[copy_len] = '\0';
    if(0 != OpenerNbIfaceSetHostName(ifnum, host_name_buf)) {
      OPENER_TRACE_ERR("OpenerNbApplyTcpIpConfiguration: failed to set host name\n");
      return kEipStatusError;
    }
  }

  if(0 != OpenerNbIfaceCommitConfig(ifnum, method)) {
    OPENER_TRACE_ERR("OpenerNbApplyTcpIpConfiguration: failed to commit interface config\n");
    return kEipStatusError;
  }

  if(kEipStatusOk != IfaceGetConfiguration(iface, &g_tcpip.interface_configuration)) {
    if(kOpenerNbIpConfigStatic == method) {
      OPENER_TRACE_ERR("OpenerNbApplyTcpIpConfiguration: static refresh failed\n");
      return kEipStatusError;
    }
    OPENER_TRACE_WARN("OpenerNbApplyTcpIpConfiguration: waiting for DHCP lease\n");
  }

  GetHostName(iface, &g_tcpip.hostname);
  OpenerNbSyncTcpIpObjectFromNndk(iface);
  g_tcpip.status &= (CipDword)(~kTcpipStatusIfaceCfgPend);
  return kEipStatusOk;
}
