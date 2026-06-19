/*******************************************************************************
 * NetBurner interface helpers for OpENer network configuration.
 ******************************************************************************/

#include "nb_ifconfig.h"

#include <nettypes.h>

extern IPADDR4 InterfaceIP(int interface);
extern IPADDR4 InterfaceMASK(int interface);
extern IPADDR4 InterfaceGate(int interface);
extern IPADDR4 InterfaceDNS(int interface);
extern IPADDR4 InterfaceDNS2(int interface);
extern MACADR InterfaceMAC(int interface);

extern "C" int nb_iface_number_from_name(const char *iface_name) {
  if (iface_name == NULL) {
    return 0;
  }

  int ifnum = 0;
  const char *cursor = iface_name;
  while ((*cursor >= '0') && (*cursor <= '9')) {
    ifnum = (ifnum * 10) + (*cursor - '0');
    ++cursor;
  }

  if (cursor == iface_name) {
    return 0;
  }
  return ifnum;
}

extern "C" int nb_iface_get_mac(int ifnum, uint8_t *mac_out) {
  MACADR mac = InterfaceMAC(ifnum);
  if ((mac_out == NULL) || mac.IsNull()) {
    return -1;
  }

  for (int i = 0; i < 6; ++i) {
    mac_out[i] = mac.GetByte(i);
  }
  return 0;
}

extern "C" int nb_iface_get_ipv4_config(int ifnum, NbIpv4Config *cfg_out) {
  if ((cfg_out == NULL) || (ifnum <= 0)) {
    return -1;
  }

  const IPADDR4 ip = InterfaceIP(ifnum);
  const IPADDR4 mask = InterfaceMASK(ifnum);

  if (ip.IsNull()) {
    return -1;
  }

  cfg_out->ip = (uint32_t)ip;
  cfg_out->mask = (uint32_t)mask;

  const IPADDR4 gate = InterfaceGate(ifnum);
  cfg_out->gateway = gate.IsNull() ? 0U : (uint32_t)gate;

  const IPADDR4 dns = InterfaceDNS(ifnum);
  cfg_out->dns1 = dns.IsNull() ? 0U : (uint32_t)dns;

  const IPADDR4 dns2 = InterfaceDNS2(ifnum);
  cfg_out->dns2 = dns2.IsNull() ? 0U : (uint32_t)dns2;

  return 0;
}
