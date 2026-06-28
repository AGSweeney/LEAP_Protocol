/*******************************************************************************
 * OpENer_uC-NetBurner timer HAL — microsecond monotonic clock for ACD and connection timing.
 ******************************************************************************/

#ifndef OPENER_TIMER_HAL_H_
#define OPENER_TIMER_HAL_H_

#include "opener_hal_types.h"

/**
 * @brief Initialize the hardware timer used for monotonic timestamps.
 * @return kOpenerHalOk on success.
 */
OpenerHalStatus OpenerHal_TimerInit(void);

/**
 * @brief Read monotonic time in microseconds.
 *
 * Must be suitable for Address Conflict Detection (ACD) and connection
 * timeout measurement. Does not need to represent wall-clock time.
 */
OpenerHalTimestampUs OpenerHal_TimerGetMicroseconds(void);

/** Convenience wrapper — millisecond resolution derived from microseconds. */
static inline MilliSeconds OpenerHal_TimerGetMilliseconds(void) {
  return (MilliSeconds)(OpenerHal_TimerGetMicroseconds() / 1000ULL);
}

#endif /* OPENER_TIMER_HAL_H_ */
