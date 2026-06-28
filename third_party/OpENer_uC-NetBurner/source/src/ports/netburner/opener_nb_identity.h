#ifndef OPENER_NB_IDENTITY_H_
#define OPENER_NB_IDENTITY_H_

/** Identity snapshot for LLDP chassis/port ID TLVs. */

#include "opener_nb_platform_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenerNbLldpIdentity {
  uint16_t vendor_id;
  uint16_t device_type;
  uint16_t product_code;
  uint8_t major_revision;
  uint8_t minor_revision;
  uint32_t serial_number;
  const char *product_name;
  uint8_t mac_address[6];
  opener_nb_ipv4_t ip_address;
  const char *host_name;
} OpenerNbLldpIdentity;

void OpenerNbLldpIdentityFromGlobals(OpenerNbLldpIdentity *out);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_IDENTITY_H_ */
