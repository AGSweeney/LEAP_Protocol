/*******************************************************************************
 * Copyright (c) 2017, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "random.h"

#include "opener_mem_hal.h"

Random *RandomNew(SetSeed set_seed,
                  GetNextUInt32 get_next_uint32) {
  Random *random = (Random *)OpenerHal_MemAlloc(sizeof(Random));
  if (random != NULL) {
    *random = (Random){ .set_seed = set_seed, .get_next_uint32 = get_next_uint32 };
  }
  return random;
}

void RandomDelete(Random **random) {
  OpenerHal_MemFree(*random);
  *random = NULL;
}
