/******************************************************************************
 * Ethernet Link counter callbacks - refresh from NetBurner FEC MIB hardware.
 ******************************************************************************/

#include "ethlinkcbs.h"

#include <string.h>

#include "cipcommon.h"
#include "cipethernetlink.h"
#include "opener_api.h"
#include "nb_eth_counters.h"
#include "trace.h"

static CipUsint s_interface_state[OPENER_ETHLINK_INSTANCE_CNT];

void CipEthernetLinkSetInterfaceState(CipInstanceNum instance,
                                      CipEthernetLinkInterfaceState state) {
  if ((instance == 0U) || (instance > OPENER_ETHLINK_INSTANCE_CNT)) {
    return;
  }
  s_interface_state[instance - 1U] = (CipUsint)state;
}

void EthLnkRegisterInterfaceState(void) {
  CipClass *eth_link_class = GetCipClass(kCipEthernetLinkClassCode);
  int idx;

  for (idx = 0; idx < OPENER_ETHLINK_INSTANCE_CNT; ++idx) {
    CipInstance *eth_link_inst =
      GetCipInstance(eth_link_class, (CipInstanceNum)(idx + 1));
    CipAttributeStruct *eth_link_attr;

    OPENER_ASSERT(eth_link_inst != NULL);
    eth_link_attr = GetCipAttribute(eth_link_inst, 8);
    if (eth_link_attr != NULL) {
      eth_link_attr->data = &s_interface_state[idx];
    }
    s_interface_state[idx] = kEthLinkInterfaceStateUnknown;
  }
}

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE

EipStatus EthLnkPreGetCallback(CipInstance *const instance,
                               CipAttributeStruct *const attribute,
                               CipByte service) {
  const CipUint attr_no = attribute->attribute_number;
  const unsigned eth_link_instance = (unsigned)instance->instance_number;

  if (attr_no == 4U) {
    (void)NbEthReadInterfaceCounters(
      eth_link_instance,
      g_ethernet_link[eth_link_instance - 1U].interface_cntrs.cntr32);
  } else if (attr_no == 5U) {
    (void)NbEthReadMediaCounters(
      eth_link_instance,
      g_ethernet_link[eth_link_instance - 1U].media_cntrs.cntr32);
  }

  (void)service;
  return kEipStatusOk;
}

EipStatus EthLnkPostGetCallback(CipInstance *const instance,
                                CipAttributeStruct *const attribute,
                                CipByte service) {
  const unsigned eth_link_instance = (unsigned)instance->instance_number;

  if (kEthLinkGetAndClear == (service & 0x7FU)) {
    if (attribute->attribute_number == 4U) {
      NbEthClearInterfaceCounters(eth_link_instance);
      memset(&g_ethernet_link[eth_link_instance - 1U].interface_cntrs, 0,
             sizeof(g_ethernet_link[eth_link_instance - 1U].interface_cntrs));
    } else if (attribute->attribute_number == 5U) {
      NbEthClearMediaCounters(eth_link_instance);
      memset(&g_ethernet_link[eth_link_instance - 1U].media_cntrs, 0,
             sizeof(g_ethernet_link[eth_link_instance - 1U].media_cntrs));
    }
  }

  return kEipStatusOk;
}

#else

EipStatus EthLnkPreGetCallback(CipInstance *const instance,
                               CipAttributeStruct *const attribute,
                               CipByte service) {
  (void)instance;
  (void)attribute;
  (void)service;
  return kEipStatusOk;
}

EipStatus EthLnkPostGetCallback(CipInstance *const instance,
                                CipAttributeStruct *const attribute,
                                CipByte service) {
  (void)instance;
  (void)attribute;
  (void)service;
  return kEipStatusOk;
}

#endif /* OPENER_ETHLINK_CNTRS_ENABLE */
