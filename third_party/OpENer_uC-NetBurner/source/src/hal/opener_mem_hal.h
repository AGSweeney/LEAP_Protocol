/*******************************************************************************
 * OpENer_uC-NetBurner — static memory pool HAL
 *
 * All CipCalloc/CipFree and RandomNew paths use this pool on embedded builds.
 * Increase OPENER_HAL_MEM_POOL_SIZE in CMake if Forward Open fails with OOM.
 *
 * Call OpenerHal_MemInit() once before CipStackInit() (OpenerNbPrepareNetworkStack does this).
 ******************************************************************************/

#ifndef OPENER_MEM_HAL_H_
#define OPENER_MEM_HAL_H_

#include <stddef.h>
#include "opener_hal_types.h"

/** @brief Initialize static pool; safe to call once at boot. */
OpenerHalStatus OpenerHal_MemInit(void);

/** @brief Raw allocation from pool (prefer OpenerHal_MemCalloc for CIP objects). */
void *OpenerHal_MemAlloc(size_t size);

/** @brief Zero-filled allocation — backs CipCalloc(). */
void *OpenerHal_MemCalloc(size_t count, size_t size);

/** @brief Return block to pool — backs CipFree(). */
void OpenerHal_MemFree(void *pointer);

/** Total bytes reserved for the static pool (compile-time budget). */
#ifndef OPENER_HAL_MEM_POOL_SIZE
#define OPENER_HAL_MEM_POOL_SIZE (16U * 1024U)
#endif

#endif /* OPENER_MEM_HAL_H_ */
