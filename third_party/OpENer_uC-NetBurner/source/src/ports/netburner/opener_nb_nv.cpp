/*******************************************************************************
 * OpENer_uC-NetBurner — ACD NV blob in HalStore_UserParams (offset 512)
 ******************************************************************************/

#include "opener_nb_nv.h"

#if OPENER_NB_ACD

#include "opener_nb_hal_storage.h"
#include <string.h>

#define OPENER_NB_ACD_NV_MAGIC   0x53434144u
#define OPENER_NB_ACD_NV_OFFSET  512

typedef struct opener_nb_acd_nv_blob {
  uint32_t magic;
  uint8_t select_acd;
  uint8_t reserved[3];
  OpenerNbAcdLastConflict last_conflict;
} opener_nb_acd_nv_blob_t;

static opener_nb_acd_nv_blob_t g_acd_nv;

static void acd_nv_set_defaults(void)
{
  memset(&g_acd_nv, 0, sizeof(g_acd_nv));
  g_acd_nv.magic = OPENER_NB_ACD_NV_MAGIC;
  g_acd_nv.select_acd = OPENER_NB_ACD_DEFAULT_SELECT ? 1u : 0u;
}

void OpenerNbAcdNvLoad(void)
{
  acd_nv_set_defaults();

  opener_nb_acd_nv_blob_t blob;
  const int n = HalStorage_Read(HalStore_UserParams, &blob, (int)sizeof(blob),
                                OPENER_NB_ACD_NV_OFFSET);
  if(n == (int)sizeof(blob) && blob.magic == OPENER_NB_ACD_NV_MAGIC) {
    g_acd_nv = blob;
  }
}

void OpenerNbAcdNvSave(void)
{
  g_acd_nv.magic = OPENER_NB_ACD_NV_MAGIC;
  (void)HalStorage_Save(HalStore_UserParams, &g_acd_nv, (int)sizeof(g_acd_nv),
                        OPENER_NB_ACD_NV_OFFSET);
}

bool OpenerNbAcdNvGetSelectAcd(void)
{
  return g_acd_nv.select_acd != 0u;
}

void OpenerNbAcdNvSetSelectAcd(bool enable)
{
  g_acd_nv.select_acd = enable ? 1u : 0u;
}

void OpenerNbAcdNvGetLastConflict(OpenerNbAcdLastConflict *out)
{
  if(NULL == out) {
    return;
  }
  *out = g_acd_nv.last_conflict;
}

void OpenerNbAcdNvSetLastConflict(const OpenerNbAcdLastConflict *in)
{
  if(NULL == in) {
    memset(&g_acd_nv.last_conflict, 0, sizeof(g_acd_nv.last_conflict));
  } else {
    g_acd_nv.last_conflict = *in;
  }
}

#endif /* OPENER_NB_ACD */
