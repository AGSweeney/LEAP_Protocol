/*******************************************************************************
 * NetBurner network configuration for LEAP Gateway OpENer.
 ******************************************************************************/

#include <string.h>

#include "cipstring.h"
#include "networkconfig.h"
#include "nb_ifconfig.h"
#include "trace.h"
#include "opener_api.h"

EipStatus IfaceGetMacAddress(const char *iface,
                             uint8_t *const physical_address) {
  const int ifnum = nb_iface_number_from_name(iface);
  return (nb_iface_get_mac(ifnum, physical_address) == 0) ? kEipStatusOk : kEipStatusError;
}

EipStatus IfaceGetConfiguration(const char *iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg) {
  CipTcpIpInterfaceConfiguration local_cfg;
  const int ifnum = nb_iface_number_from_name(iface);
  NbIpv4Config ipv4_cfg;

  memset(&local_cfg, 0, sizeof(local_cfg));
  memset(&ipv4_cfg, 0, sizeof(ipv4_cfg));
  if (nb_iface_get_ipv4_config(ifnum, &ipv4_cfg) != 0) {
    return kEipStatusError;
  }

  local_cfg.ip_address = ipv4_cfg.ip;
  local_cfg.network_mask = ipv4_cfg.mask;
  local_cfg.gateway = ipv4_cfg.gateway;
  local_cfg.name_server = ipv4_cfg.dns1;
  local_cfg.name_server_2 = ipv4_cfg.dns2;
  ClearCipString(&iface_cfg->domain_name);
  *iface_cfg = local_cfg;
  return kEipStatusOk;
}

EipStatus IfaceWaitForIp(const char *const iface,
                         int timeout,
                         volatile int *const abort_wait) {
  (void)iface;
  (void)timeout;
  (void)abort_wait;
  return kEipStatusOk;
}

void GetHostName(CipString *hostname) {
  SetCipStringByCstr(hostname, "LEAP-Gateway");
}
