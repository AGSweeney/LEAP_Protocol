/*******************************************************************************
 * Shared NetBurner interface helpers for OpENer (C/C++ boundary).
 ******************************************************************************/

#ifndef NB_IFCONFIG_H_
#define NB_IFCONFIG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NbIpv4Config {
  uint32_t ip;
  uint32_t mask;
  uint32_t gateway;
  uint32_t dns1;
  uint32_t dns2;
} NbIpv4Config;

int nb_iface_number_from_name(const char *iface_name);
int nb_iface_get_mac(int ifnum, uint8_t *mac_out);
int nb_iface_get_ipv4_config(int ifnum, NbIpv4Config *cfg_out);

#ifdef __cplusplus
}
#endif

#endif /* NB_IFCONFIG_H_ */
