/*******************************************************************************
 * OpENer_uC-NetBurner — NNDK interface queries for HAL and CIP network objects
 *
 * All public functions are declared in opener_nb_ifconfig.h. IPv4 byte order
 * conversion matches CIP TCP/IP object layout (CipUdint).
 ******************************************************************************/

#include "opener_nb_ifconfig.h"

#include <stdint.h>
#include <string.h>

#include <config_server.h>
#include <nettypes.h>
#include <netinterface.h>
#include <counters.h>

extern "C" {
#include "cipethernetlink.h"
}

extern IPADDR4 InterfaceIP(int interface);
extern IPADDR4 InterfaceMASK(int interface);
extern IPADDR4 InterfaceGate(int interface);
extern IPADDR4 InterfaceDNS(int interface);
extern IPADDR4 InterfaceDNS2(int interface);
extern MACADR InterfaceMAC(int interface);

typedef struct OpenerNbEthCounterSnapshot {
  uint32_t in_octets;
  uint32_t in_ucast;
  uint32_t in_nucast;
  uint32_t in_discards;
  uint32_t in_errors;
  uint32_t in_unknown_protos;
  uint32_t out_octets;
  uint32_t out_ucast;
  uint32_t out_nucast;
  uint32_t out_discards;
  uint32_t out_errors;
} OpenerNbEthCounterSnapshot;

static int s_last_eth_counter_ifnum = 1;

static void OpenerNbReadEthCounterSnapshot(int ifnum, OpenerNbEthCounterSnapshot *out) {
  if(NULL == out) {
    return;
  }

  memset(out, 0, sizeof(*out));

#if defined(ENABLE_SNMP)
  InterfaceBlock *ib = GetInterfaceBlock(ifnum);
  if(NULL != ib) {
    out->in_octets = (uint32_t)ib->EthifInOctets;
    out->in_ucast = (uint32_t)ib->EthifInUcastPkts;
    out->in_nucast = (uint32_t)ib->EthifInNUcastPkts;
    out->in_discards = (uint32_t)ib->EthifInDiscards;
    out->in_errors = (uint32_t)ib->EthifInErrors;
    out->in_unknown_protos = (uint32_t)ib->EthifInUnknownProtos;
    out->out_octets = (uint32_t)ib->EthifOutOctets;
    out->out_ucast = (uint32_t)ib->EthifOutUcastPkts;
    out->out_nucast = (uint32_t)ib->EthifOutNUcastPkts;
    out->out_discards = (uint32_t)ib->EthifOutDiscards;
    /* Do not expose EthifOutErrors: MIMXRT10xx can inflate this in normal TX ISR paths. */
    out->out_errors = 0U;
    return;
  }
#endif

  /* Coarse fallback when SNMP interface counters are unavailable. */
  out->in_ucast = (uint32_t)frames_rx;
  out->in_discards = (uint32_t)frames_rx_discard + (uint32_t)frames_ip_discard;
  out->in_errors = (uint32_t)frames_rx_err;
  out->in_unknown_protos = (uint32_t)frames_rx_unknown;
  out->out_ucast = (uint32_t)frames_tx;
  /* Octets + non-unicast detail + out discards are not reliably exposed in this mode. */
  out->out_errors = 0U;
}

static void OpenerNbClearSnmpEthernetCounters(int ifnum) {
#if defined(ENABLE_SNMP)
  InterfaceBlock *ib = GetInterfaceBlock(ifnum);
  if(NULL == ib) {
    return;
  }

  ib->EthifInOctets = 0U;
  ib->EthifInUcastPkts = 0U;
  ib->EthifInNUcastPkts = 0U;
  ib->EthifInDiscards = 0U;
  ib->EthifInErrors = 0U;
  ib->EthifInUnknownProtos = 0U;
  ib->EthifOutOctets = 0U;
  ib->EthifOutUcastPkts = 0U;
  ib->EthifOutNUcastPkts = 0U;
  ib->EthifOutDiscards = 0U;
#else
  (void)ifnum;
#endif
}

static uint32_t nb_ipv4_to_cip(uint32_t value) {
  return ((value & 0x000000FFUL) << 24) |
         ((value & 0x0000FF00UL) << 8) |
         ((value & 0x00FF0000UL) >> 8) |
         ((value & 0xFF000000UL) >> 24);
}

extern "C" uint32_t OpenerNbIpv4ToCip(uint32_t nb_ipv4) {
  return nb_ipv4_to_cip(nb_ipv4);
}

extern "C" uint32_t OpenerNbCipToIpv4(uint32_t cip_ipv4) {
  return nb_ipv4_to_cip(cip_ipv4);
}

extern "C" int OpenerNbIfaceNumberFromName(const char *iface_name) {
  if(NULL == iface_name) {
    return 0;
  }

  int ifnum = 0;
  const char *cursor = iface_name;
  while((*cursor >= '0') && (*cursor <= '9')) {
    ifnum = (ifnum * 10) + (*cursor - '0');
    ++cursor;
  }

  if(cursor == iface_name) {
    return 0;
  }
  return ifnum;
}

extern "C" int OpenerNbIfaceGetMac(int ifnum, uint8_t *mac_out) {
  MACADR mac = InterfaceMAC(ifnum);
  if((NULL == mac_out) || mac.IsNull()) {
    return -1;
  }

  for(int i = 0; i < 6; ++i) {
    mac_out[i] = mac.GetByte(i);
  }
  return 0;
}

extern "C" int OpenerNbIfaceGetIpv4Config(int ifnum, OpenerNbIpv4Config *cfg_out) {
  if((NULL == cfg_out) || (ifnum <= 0)) {
    return -1;
  }

  const IPADDR4 ip = InterfaceIP(ifnum);
  const IPADDR4 mask = InterfaceMASK(ifnum);

  if(ip.IsNull()) {
    return -1;
  }

  cfg_out->ip = nb_ipv4_to_cip((uint32_t)ip);
  cfg_out->mask = nb_ipv4_to_cip((uint32_t)mask);

  const IPADDR4 gate = InterfaceGate(ifnum);
  cfg_out->gateway = gate.IsNull() ? 0U : nb_ipv4_to_cip((uint32_t)gate);

  const IPADDR4 dns = InterfaceDNS(ifnum);
  cfg_out->dns1 = dns.IsNull() ? 0U : nb_ipv4_to_cip((uint32_t)dns);

  const IPADDR4 dns2 = InterfaceDNS2(ifnum);
  cfg_out->dns2 = dns2.IsNull() ? 0U : nb_ipv4_to_cip((uint32_t)dns2);

  return 0;
}

extern "C" OpenerNbIpConfigMethod OpenerNbIfaceGetIpConfigMethod(int ifnum) {
  if(ifnum <= 0) {
    return kOpenerNbIpConfigUnknown;
  }

  InterfaceBlock *ib = GetInterfaceBlock(ifnum);
  if(NULL == ib) {
    return kOpenerNbIpConfigUnknown;
  }

  if(ib->ip4.mode.IsSelected("Static")) {
    return kOpenerNbIpConfigStatic;
  }
  if(ib->ip4.mode.IsSelected("DHCP") ||
     ib->ip4.mode.IsSelected("DHCP w Fallback")) {
    return kOpenerNbIpConfigDhcp;
  }
  return kOpenerNbIpConfigUnknown;
}

extern "C" int OpenerNbIfaceGetHostName(int ifnum, char *buf, size_t buf_len) {
  if((NULL == buf) || (0U == buf_len) || (ifnum <= 0)) {
    return -1;
  }

  const char *name = NULL;
  InterfaceBlock *ib = GetInterfaceBlock(ifnum);

  if((NULL != ib) && ('\0' != ib->device_name.c_str()[0])) {
    name = ib->device_name.c_str();
  }
  if((NULL == name) || ('\0' == name[0])) {
    name = InterfaceName(ifnum);
  }
  if(((NULL == name) || ('\0' == name[0])) &&
     (NULL != ib) && (NULL != ib->GetInterfaceName())) {
    name = ib->GetInterfaceName();
  }

  if((NULL == name) || ('\0' == name[0])) {
    return -1;
  }

  strncpy(buf, name, buf_len - 1U);
  buf[buf_len - 1U] = '\0';
  return 0;
}

extern "C" int OpenerNbIfaceSetIpv4Config(int ifnum, const OpenerNbIpv4Config *cfg_in) {
  if((ifnum <= 0) || (NULL == cfg_in)) {
    return -1;
  }

  InterfaceBlock *ib = GetInterfaceBlock(ifnum);
  if(NULL == ib) {
    return -1;
  }

  ib->ip4.addr = IPADDR4(OpenerNbCipToIpv4(cfg_in->ip));
  ib->ip4.mask = IPADDR4(OpenerNbCipToIpv4(cfg_in->mask));
  ib->ip4.gate = IPADDR4(OpenerNbCipToIpv4(cfg_in->gateway));
  ib->ip4.dns1 = IPADDR4(OpenerNbCipToIpv4(cfg_in->dns1));
  ib->ip4.dns2 = IPADDR4(OpenerNbCipToIpv4(cfg_in->dns2));
  return 0;
}

extern "C" int OpenerNbIfaceSetIpConfigMethod(int ifnum, OpenerNbIpConfigMethod method) {
  if(ifnum <= 0) {
    return -1;
  }

  InterfaceBlock *ib = GetInterfaceBlock(ifnum);
  if(NULL == ib) {
    return -1;
  }

  switch(method) {
    case kOpenerNbIpConfigStatic:
      ib->ip4.mode = "Static";
      return 0;
    case kOpenerNbIpConfigDhcp:
      ib->ip4.mode = "DHCP";
      return 0;
    default:
      return -1;
  }
}

extern "C" int OpenerNbIfaceSetHostName(int ifnum, const char *host_name) {
  if((ifnum <= 0) || (NULL == host_name) || ('\0' == host_name[0])) {
    return -1;
  }

  InterfaceBlock *ib = GetInterfaceBlock(ifnum);
  if(NULL == ib) {
    return -1;
  }

  ib->device_name = host_name;
  return 0;
}

extern "C" int OpenerNbIfaceCommitConfig(int ifnum, OpenerNbIpConfigMethod method) {
  if(ifnum <= 0) {
    return -1;
  }

  InterfaceBlock *ib = GetInterfaceBlock(ifnum);
  if(NULL == ib) {
    return -1;
  }

  SaveConfigToStorage();

  switch(method) {
    case kOpenerNbIpConfigStatic:
      ib->dhcpClient.StopDHCP();
      ib->ip4.cur_addr.i4 = ib->ip4.addr;
      ib->ip4.cur_mask.i4 = ib->ip4.mask;
      ib->ip4.cur_gate.i4 = ib->ip4.gate;
      ib->ip4.cur_dns1.i4 = ib->ip4.dns1;
      return 0;
    case kOpenerNbIpConfigDhcp:
      ib->dhcpClient.RestartDHCP();
      return 0;
    default:
      return -1;
  }
}

extern "C" void OpenerNbSyncEthernetLinkFromNndk(int ifnum) {
  if(ifnum <= 0) {
    return;
  }

  s_last_eth_counter_ifnum = ifnum;
  const bool link_up = InterfaceLinkActive(ifnum);
  const int speed = InterfaceLinkSpeed(ifnum);
  const bool full_duplex = InterfaceLinkDuplex(ifnum);
  CipEthernetLinkUpdateLinkStatus((CipUdint)(speed > 0 ? speed : 100),
                                  full_duplex ? 1U : 0U,
                                  link_up ? 1U : 0U);

  OpenerNbEthCounterSnapshot current_counters;
  OpenerNbReadEthCounterSnapshot(ifnum, &current_counters);

  CipEthernetLinkUpdateInterfaceCounters(
    (CipUdint)current_counters.in_octets,
    (CipUdint)current_counters.in_ucast,
    (CipUdint)current_counters.in_nucast,
    (CipUdint)current_counters.in_discards,
    (CipUdint)current_counters.in_errors,
    (CipUdint)current_counters.in_unknown_protos,
    (CipUdint)current_counters.out_octets,
    (CipUdint)current_counters.out_ucast,
    (CipUdint)current_counters.out_nucast,
    (CipUdint)current_counters.out_discards,
    (CipUdint)current_counters.out_errors);
}

extern "C" void OpenerEthernetLinkPlatformClearCounters(CipInstanceNum instance_number,
                                                         EipUint16 attribute_number) {
  (void)instance_number;
  if((4U != attribute_number) && (5U != attribute_number)) {
    return;
  }

#if defined(ENABLE_SNMP)
  OpenerNbClearSnmpEthernetCounters(s_last_eth_counter_ifnum);
  CipEthernetLinkUpdateInterfaceCounters(0U, 0U, 0U, 0U, 0U, 0U,
                                         0U, 0U, 0U, 0U, 0U);
#endif
}
