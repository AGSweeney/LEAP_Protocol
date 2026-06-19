/******************************************************************************
 * Ethernet Link object callbacks for LeapGateway.
 ******************************************************************************/

#ifndef OPENER_ETHLINKCBS_H_
#define OPENER_ETHLINKCBS_H_

#include "typedefs.h"
#include "ciptypes.h"

typedef enum {
  kEthLinkInterfaceStateUnknown = 0,
  kEthLinkInterfaceStateEnabled = 1,
  kEthLinkInterfaceStateDisabled = 2,
  kEthLinkInterfaceStateTesting = 3
} CipEthernetLinkInterfaceState;

EipStatus EthLnkPreGetCallback(CipInstance *const instance,
                               CipAttributeStruct *const attribute,
                               CipByte service);

EipStatus EthLnkPostGetCallback(CipInstance *const instance,
                                CipAttributeStruct *const attribute,
                                CipByte service);

void CipEthernetLinkSetInterfaceState(CipInstanceNum instance,
                                      CipEthernetLinkInterfaceState state);

void EthLnkRegisterInterfaceState(void);

#endif /* OPENER_ETHLINKCBS_H_ */
