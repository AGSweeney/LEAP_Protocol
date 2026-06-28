#ifndef OPENER_NB_NV_H_
#define OPENER_NB_NV_H_

/** ACD NV block in HalStore_UserParams (offset 512). */

#include <stdbool.h>
#include <stdint.h>

#include "opener_nb_acd_config.h"

#if OPENER_NB_ACD

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenerNbAcdLastConflict {
  uint8_t acd_activity;
  uint8_t remote_mac[6];
  uint8_t arp_pdu[28];
} OpenerNbAcdLastConflict;

#define OPENER_NB_ACD_ACTIVITY_NONE        0u
#define OPENER_NB_ACD_ACTIVITY_PROBE       1u
#define OPENER_NB_ACD_ACTIVITY_ONGOING     2u
#define OPENER_NB_ACD_ACTIVITY_SEMI_ACTIVE 3u

void OpenerNbAcdNvLoad(void);
void OpenerNbAcdNvSave(void);
bool OpenerNbAcdNvGetSelectAcd(void);
void OpenerNbAcdNvSetSelectAcd(bool enable);
void OpenerNbAcdNvGetLastConflict(OpenerNbAcdLastConflict *out);
void OpenerNbAcdNvSetLastConflict(const OpenerNbAcdLastConflict *in);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_ACD */

#endif /* OPENER_NB_NV_H_ */
