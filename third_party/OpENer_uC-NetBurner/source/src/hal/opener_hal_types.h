/*******************************************************************************
 * OpENer_uC-NetBurner embedded HAL — shared type definitions
 *
 * The protocol core communicates with hardware only through opener_net_hal.h,
 * opener_timer_hal.h, and opener_mem_hal.h.
 *
 * Integration:
 *   OpenerNetIfHandle  — cast NNDK 1-based ifnum: (OpenerNetIfHandle)(intptr_t)1
 *   OpenerSocketHandle — NNDK socket fd (int)
 *   OpenerHalStatus    — kOpenerHalWouldBlock drives non-blocking network loop
 ******************************************************************************/

#ifndef OPENER_HAL_TYPES_H_
#define OPENER_HAL_TYPES_H_

#include "typedefs.h"

/** Opaque network interface handle (NetBurner: 1-based interface number). */
typedef void *OpenerNetIfHandle;

/** Socket handle returned by the network HAL. */
typedef int OpenerSocketHandle;

#define OPENER_HAL_INVALID_SOCKET (-1)

/** Monotonic timestamp in microseconds (required for ACD and connection timers). */
typedef MicroSeconds OpenerHalTimestampUs;

/** Result codes for non-blocking socket operations. */
typedef enum {
  kOpenerHalOk = 0,
  kOpenerHalError = -1,
  kOpenerHalWouldBlock = -2,
  kOpenerHalTimeout = -3
} OpenerHalStatus;

/** IPv4 endpoint in host byte order unless noted. */
typedef struct {
  CipUdint address;
  CipUint port;
} OpenerHalEndpoint;

/**
 * Zero-copy receive view into an NNDK buffer (optional; not implemented yet).
 * The HAL owns the backing storage until OpenerHal_BufferRelease() is called.
 */
typedef struct {
  const CipOctet *data;
  size_t length;
  void *platform_token;
} OpenerHalBufferView;

#endif /* OPENER_HAL_TYPES_H_ */
