#ifndef NB_ETH_COUNTERS_H_
#define NB_ETH_COUNTERS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Layout matches CipEthernetLink{Interface,Media}Counters::cntr32. */
int NbEthReadInterfaceCounters(unsigned eth_link_instance, uint32_t *cntr32_11);
int NbEthReadMediaCounters(unsigned eth_link_instance, uint32_t *cntr32_12);
void NbEthClearInterfaceCounters(unsigned eth_link_instance);
void NbEthClearMediaCounters(unsigned eth_link_instance);

#ifdef __cplusplus
}
#endif

#endif /* NB_ETH_COUNTERS_H_ */
