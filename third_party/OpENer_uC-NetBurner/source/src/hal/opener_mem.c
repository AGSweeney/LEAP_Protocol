/*******************************************************************************
 * OpENer_uC-NetBurner — static memory pool (replaces heap malloc/free in CIP path)
 *
 * Sized by OPENER_HAL_MEM_POOL_SIZE in CMake. Increase if Forward Open or File Object
 * init fails silently — see source/doc/memory/README.md.
 ******************************************************************************/

#include "opener_mem_hal.h"

#include <stdint.h>
#include <string.h>

typedef struct MemBlockHeader {
  size_t size;
  struct MemBlockHeader *next;
} MemBlockHeader;

static uint8_t s_mem_pool[OPENER_HAL_MEM_POOL_SIZE];
static MemBlockHeader *s_free_list = NULL;
static int s_mem_initialized = 0;

static void MemInsertFreeBlock(MemBlockHeader *block) {
  block->next = s_free_list;
  s_free_list = block;
}

OpenerHalStatus OpenerHal_MemInit(void) {
  if (s_mem_initialized) {
    return kOpenerHalOk;
  }
  MemBlockHeader *initial = (MemBlockHeader *)s_mem_pool;
  initial->size = OPENER_HAL_MEM_POOL_SIZE - sizeof(MemBlockHeader);
  initial->next = NULL;
  s_free_list = initial;
  s_mem_initialized = 1;
  return kOpenerHalOk;
}

static MemBlockHeader *MemFindBlock(size_t requested_size) {
  MemBlockHeader *previous = NULL;
  MemBlockHeader *current = s_free_list;

  while (current != NULL) {
    if (current->size >= requested_size) {
      if (current->size >= requested_size + sizeof(MemBlockHeader) + 16U) {
        uint8_t *split_address = (uint8_t *)current + sizeof(MemBlockHeader) + requested_size;
        MemBlockHeader *remainder = (MemBlockHeader *)split_address;
        remainder->size = current->size - requested_size - sizeof(MemBlockHeader);
        remainder->next = current->next;
        current->size = requested_size;
        if (previous != NULL) {
          previous->next = remainder;
        } else {
          s_free_list = remainder;
        }
      } else if (previous != NULL) {
        previous->next = current->next;
      } else {
        s_free_list = current->next;
      }
      return current;
    }
    previous = current;
    current = current->next;
  }
  return NULL;
}

void *OpenerHal_MemAlloc(size_t size) {
  if (!s_mem_initialized) {
    (void)OpenerHal_MemInit();
  }
  if (size == 0U) {
    return NULL;
  }
  MemBlockHeader *block = MemFindBlock(size);
  if (block == NULL) {
    return NULL;
  }
  return (void *)((uint8_t *)block + sizeof(MemBlockHeader));
}

void *OpenerHal_MemCalloc(size_t count, size_t size) {
  if ((count == 0U) || (size == 0U)) {
    return NULL;
  }
  const size_t total = count * size;
  void *pointer = OpenerHal_MemAlloc(total);
  if (pointer != NULL) {
    memset(pointer, 0, total);
  }
  return pointer;
}

void OpenerHal_MemFree(void *pointer) {
  if (pointer == NULL) {
    return;
  }
  MemBlockHeader *block = (MemBlockHeader *)((uint8_t *)pointer - sizeof(MemBlockHeader));
  MemInsertFreeBlock(block);
}
