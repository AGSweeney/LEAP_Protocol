/*******************************************************************************
 * OpENer_uC-NetBurner — monotonic timer HAL (NNDK TimeTick)
 *
 * OpenerHal_TimerGetMicroseconds() backs connection RPI timers and ACD probe timing.
 * Default NNDK tick is 50 ms (TICKS_PER_SECOND=20).
 *
 * OpenerHal_TimerInit() is a no-op — NNDK tick source is initialized by init().
 ******************************************************************************/

#include "opener_timer_hal.h"

#include <stdint.h>

extern volatile uint32_t TimeTick;

#ifndef TICKS_PER_SECOND
#define TICKS_PER_SECOND 20U
#endif

OpenerHalStatus OpenerHal_TimerInit(void) {
  return kOpenerHalOk;
}

OpenerHalTimestampUs OpenerHal_TimerGetMicroseconds(void) {
  const uint64_t ticks = (uint64_t)TimeTick;
  const uint64_t usec_per_tick = 1000000ULL / (uint64_t)TICKS_PER_SECOND;
  return (OpenerHalTimestampUs)(ticks * usec_per_tick);
}
