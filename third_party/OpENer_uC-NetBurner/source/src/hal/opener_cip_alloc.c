/*******************************************************************************
 * OpENer_uC-NetBurner — CipCalloc/CipFree routed through OpenerHal_MemCalloc/MemFree
 *
 * All portable core allocations use these entry points. Do not replace with libc
 * malloc in product firmware unless you also replace OpenerHal_Mem*.
 ******************************************************************************/

#include "opener_api.h"

#include "opener_mem_hal.h"

void *CipCalloc(size_t number_of_elements, size_t size_of_element) {
  return OpenerHal_MemCalloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
  OpenerHal_MemFree(data);
}
