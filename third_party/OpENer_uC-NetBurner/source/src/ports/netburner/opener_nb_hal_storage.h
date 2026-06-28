#ifndef OPENER_NB_HAL_STORAGE_H_
#define OPENER_NB_HAL_STORAGE_H_

/*******************************************************************************
 * OpENer_uC-NetBurner — NNDK non-volatile storage declarations
 *
 * Symbols are provided by libnetburner.a. Used by opener_nb_nv.cpp for ACD and
 * LLDP parameter persistence in HalStore_UserParams (area 0x03).
 *
 * Offsets (see opener_nb_nv.cpp / opener_nb_lldp.cpp):
 *   512 — ACD select + last conflict blob
 *   LLDP blob — management enable/interval/hold (after ACD region)
 ******************************************************************************/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum HalStorage_t {
  HalStore_UserParams = 0x03
};

/** @brief Read bytes from NNDK user parameter store. Returns bytes read or negative error. */
int HalStorage_Read(uint8_t area, void *pData, int len, int offset);

/** @brief Write bytes to NNDK user parameter store. Returns bytes written or negative error. */
int HalStorage_Save(uint8_t area, void *pData, int len, int offset);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_HAL_STORAGE_H_ */
