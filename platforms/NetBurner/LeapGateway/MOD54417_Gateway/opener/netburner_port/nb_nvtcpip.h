/*******************************************************************************
 * NetBurner flash-backed TCP/IP NV helpers (C/C++ boundary).
 ******************************************************************************/

#ifndef NB_NVTCPIP_H_
#define NB_NVTCPIP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NbTcpIpNvConfig {
  uint32_t config_control;
  uint32_t ip;
  uint32_t mask;
  uint32_t gateway;
  uint32_t dns1;
  uint32_t dns2;
} NbTcpIpNvConfig;

int nb_nv_tcpip_load(NbTcpIpNvConfig *cfg_out);
int nb_nv_tcpip_store(const NbTcpIpNvConfig *cfg_in);

#ifdef __cplusplus
}
#endif

#endif /* NB_NVTCPIP_H_ */
