/*******************************************************************************
 * Little-endian helpers for NetBurner LLDP/ACD wire modules.
 ******************************************************************************/
#ifndef OPENER_NB_LE_H_
#define OPENER_NB_LE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t opener_nb_le_read_u16(const uint8_t *p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t opener_nb_le_read_u32(const uint8_t *p)
{
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static inline void opener_nb_le_write_u16(uint8_t *p, uint16_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline void opener_nb_le_write_u32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline size_t opener_nb_strnlen(const char *s, size_t max_len)
{
  size_t n = 0;
  while(n < max_len && s[n] != '\0') {
    ++n;
  }
  return n;
}

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_LE_H_ */
