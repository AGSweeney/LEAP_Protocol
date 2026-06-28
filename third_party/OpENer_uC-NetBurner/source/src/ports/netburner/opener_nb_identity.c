/*******************************************************************************
 * OpENer_uC-NetBurner — build LLDP identity from CIP globals for OpenerNbLldpInit()
 ******************************************************************************/

#include "opener_nb_identity.h"

#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "opener_net_hal.h"
#include "opener_nb_ifconfig.h"

void OpenerNbLldpIdentityFromGlobals(OpenerNbLldpIdentity *out) {
  if(NULL == out) {
    return;
  }

  out->vendor_id = (uint16_t)g_identity.vendor_id;
  out->device_type = (uint16_t)g_identity.device_type;
  out->product_code = (uint16_t)g_identity.product_code;
  out->major_revision = g_identity.revision.major_revision;
  out->minor_revision = g_identity.revision.minor_revision;
  out->serial_number = g_identity.serial_number;
  out->product_name = (g_identity.product_name.length > 0) ?
                      (const char *)g_identity.product_name.string : NULL;
  out->ip_address = opener_nb_ipv4_from_u32_be(g_tcpip.interface_configuration.ip_address);

  if(kOpenerHalOk == OpenerHal_GetMacAddress((OpenerNetIfHandle)(intptr_t)1, out->mac_address)) {
    /* filled */
  } else {
    for(int i = 0; i < 6; ++i) {
      out->mac_address[i] = 0;
    }
  }

  out->host_name = (g_tcpip.hostname.length > 0) ?
                   (const char *)g_tcpip.hostname.string : NULL;
}
