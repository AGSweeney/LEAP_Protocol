/*******************************************************************************
 * OpENer_uC-NetBurner — NetBurner interface handle helpers
 *
 * Cast NNDK ifnum to OpenerNetIfHandle when calling opener_init():
 *   opener_init((OpenerNetIfHandle)(intptr_t)1);
 ******************************************************************************/

#ifndef OPENER_NB_CONFIG_H_
#define OPENER_NB_CONFIG_H_

#include "opener_hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default plant interface when OpenerHal_NetInit(NULL) or opener_init(NULL) is used. */
#ifndef OPENER_NB_DEFAULT_IFNUM
#define OPENER_NB_DEFAULT_IFNUM 1
#endif

/**
 * @brief Resolve OpenerNetIfHandle to NNDK 1-based interface number.
 *
 * NULL or invalid handle returns the last OpenerHal_NetInit() value or default.
 */
int OpenerNbNetifToIfnum(OpenerNetIfHandle netif);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_CONFIG_H_ */
