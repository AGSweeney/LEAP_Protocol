#ifndef OPENER_NB_ACD_H_
#define OPENER_NB_ACD_H_

/** Wire ACD (RFC 5227) for NetBurner NNDK. */

#include "opener_nb_acd_config.h"
#include "opener_nb_nv.h"
#include "opener_nb_platform_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if OPENER_NB_ACD

bool OpenerNbAcdInit(int ifnum);
void OpenerNbAcdShutdown(void);
void OpenerNbAcdPoll(void);
void OpenerNbAcdSetAdapter(void *adapter);
bool OpenerNbAcdIsCapable(void);
bool OpenerNbAcdSelectEnabled(void);
bool OpenerNbAcdStatusConflict(void);
bool OpenerNbAcdStatusFault(void);
bool OpenerNbAcdGetSelectAcd(void);
void OpenerNbAcdGetLastConflict(OpenerNbAcdLastConflict *out);
opener_nb_status_t OpenerNbAcdSetSelectAcd(bool enable, bool *needs_reset_out);
opener_nb_status_t OpenerNbAcdClearLastConflict(void);
void OpenerNbAcdNotifyIoConnection(bool active);
void OpenerNbAcdClearIoActive(void);
void OpenerNbAcdNotifyDhcpBound(int ifnum);

#else /* OPENER_NB_ACD */

static inline bool OpenerNbAcdInit(int ifnum) { (void)ifnum; return true; }
static inline void OpenerNbAcdShutdown(void) {}
static inline void OpenerNbAcdPoll(void) {}
static inline bool OpenerNbAcdIsCapable(void) { return false; }
static inline bool OpenerNbAcdStatusConflict(void) { return false; }
static inline bool OpenerNbAcdStatusFault(void) { return false; }
static inline bool OpenerNbAcdGetSelectAcd(void) { return false; }
static inline void OpenerNbAcdGetLastConflict(void *out) { (void)out; }
static inline void OpenerNbAcdNotifyIoConnection(bool active) { (void)active; }
static inline void OpenerNbAcdNotifyDhcpBound(int ifnum) { (void)ifnum; }

#endif

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_ACD_H_ */
