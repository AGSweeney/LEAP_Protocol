#ifndef OPENER_NB_LLDP_H_
#define OPENER_NB_LLDP_H_

/** Wire LLDP (802.1AB) + CIP 0x109/0x10A for NetBurner NNDK. */

#include "opener_nb_lldp_config.h"
#include "opener_nb_identity.h"
#include "opener_nb_platform_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if OPENER_NB_LLDP

bool OpenerNbLldpInit(int ifnum, const OpenerNbLldpIdentity *cfg);
void OpenerNbLldpShutdown(void);
void OpenerNbLldpPoll(void);
void OpenerNbLldpUpdateIdentity(const OpenerNbLldpIdentity *cfg);
uint32_t OpenerNbLldpGetRxFrameCount(void);
bool OpenerNbLldpBuildMgmtAttr(uint8_t attr, uint8_t *rsp, size_t cap, size_t *len_out);
bool OpenerNbLldpBuildMgmtAll(uint8_t *rsp, size_t cap, size_t *len_out);
opener_nb_status_t OpenerNbLldpSetMgmtAttr(uint8_t attr, const uint8_t *data, size_t len,
                                           uint8_t *cip_status_out);
uint16_t OpenerNbLldpGetDataTableMaxInstance(void);
uint16_t OpenerNbLldpGetNeighborCount(void);
bool OpenerNbLldpDataTableInstanceValid(uint16_t inst);
bool OpenerNbLldpBuildDataTableAttr(uint16_t inst, uint8_t attr, uint8_t *rsp, size_t cap,
                                    size_t *len_out);

#else /* OPENER_NB_LLDP */

static inline bool OpenerNbLldpInit(int ifnum, const OpenerNbLldpIdentity *cfg)
{
  (void)ifnum;
  (void)cfg;
  return true;
}
static inline void OpenerNbLldpShutdown(void) {}
static inline void OpenerNbLldpPoll(void) {}
static inline void OpenerNbLldpUpdateIdentity(const OpenerNbLldpIdentity *cfg) { (void)cfg; }

#endif

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_LLDP_H_ */
