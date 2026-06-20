/*******************************************************************************
 * NetBurner FEC MIB counter access for CIP Ethernet Link attributes 4 and 5.
 ******************************************************************************/

#include <predef.h>
#include <stdint.h>
#include <string.h>

#include "nb_eth_counters.h"

extern int g_opener_plant_ifnum;

#if defined(MODM7AE70) || defined(SAME70) || defined(CORTEX_M7)

#include <same70q21.h>
#include <instance/gmac.h>

static uint32_t NbEthSaturatingSub(uint32_t value, uint32_t minus) {
  return (value >= minus) ? (value - minus) : 0U;
}

int NbEthReadInterfaceCounters(const unsigned eth_link_instance,
                               uint32_t *const cntr32_11) {
  uint32_t in_non_ucast;
  uint32_t out_non_ucast;

  (void)eth_link_instance;
  if (cntr32_11 == NULL) {
    return -1;
  }

  in_non_ucast = REG_GMAC_BCFR + REG_GMAC_MFR;
  out_non_ucast = REG_GMAC_BCFT + REG_GMAC_MFT;

  cntr32_11[0] = REG_GMAC_ORLO;
  cntr32_11[1] = NbEthSaturatingSub(REG_GMAC_FR, in_non_ucast);
  cntr32_11[2] = in_non_ucast;
  cntr32_11[3] = REG_GMAC_RRE + REG_GMAC_ROE;
  cntr32_11[4] =
    REG_GMAC_FCSE + REG_GMAC_AE + REG_GMAC_RSE + REG_GMAC_LFFE + REG_GMAC_OFR + REG_GMAC_JR;
  cntr32_11[5] = 0U;
  cntr32_11[6] = REG_GMAC_OTLO;
  cntr32_11[7] = NbEthSaturatingSub(REG_GMAC_FT, out_non_ucast);
  cntr32_11[8] = out_non_ucast;
  cntr32_11[9] = 0U;
  cntr32_11[10] = REG_GMAC_TUR + REG_GMAC_CSE;
  return 0;
}

int NbEthReadMediaCounters(const unsigned eth_link_instance,
                           uint32_t *const cntr32_12) {
  (void)eth_link_instance;
  if (cntr32_12 == NULL) {
    return -1;
  }

  cntr32_12[0] = REG_GMAC_AE;
  cntr32_12[1] = REG_GMAC_FCSE;
  cntr32_12[2] = REG_GMAC_SCF;
  cntr32_12[3] = REG_GMAC_MCF;
  cntr32_12[4] = 0U;
  cntr32_12[5] = REG_GMAC_DTF;
  cntr32_12[6] = REG_GMAC_LC;
  cntr32_12[7] = REG_GMAC_EC;
  cntr32_12[8] = REG_GMAC_TUR;
  cntr32_12[9] = REG_GMAC_CSE;
  cntr32_12[10] = REG_GMAC_OFR + REG_GMAC_JR;
  cntr32_12[11] = REG_GMAC_RSE;
  return 0;
}

void NbEthClearInterfaceCounters(const unsigned eth_link_instance) {
  (void)eth_link_instance;
}

void NbEthClearMediaCounters(const unsigned eth_link_instance) {
  (void)eth_link_instance;
}

#else

#include <sim5441x.h>

static int NbFecnForEthLinkInstance(const unsigned eth_link_instance) {
  int ifnum;

  (void)eth_link_instance;

  if (g_opener_plant_ifnum > 0) {
    ifnum = g_opener_plant_ifnum;
  } else {
    ifnum = 1;
  }

  /* MOD54417: NetBurner interface N maps to FEC (N - 1). */
  if (ifnum <= 0) {
    return 0;
  }
  return ifnum - 1;
}

static void NbEthClearFecMibBlocks(const int fecn) {
  volatile uint32_t *pdw;
  uint32_t i;

  if ((fecn < 0) || (fecn > 1)) {
    return;
  }

  sim2.fec[fecn].mibc = 0x80000000U;

  pdw = (volatile uint32_t *)&sim2.fec[fecn].fec_rmon_t;
  for (i = 0U; i < (sizeof(sim2.fec[fecn].fec_rmon_t) / sizeof(uint32_t)); ++i) {
    pdw[i] = 0U;
  }

  pdw = (volatile uint32_t *)&sim2.fec[fecn].fec_ieee_t;
  for (i = 0U; i < (sizeof(sim2.fec[fecn].fec_ieee_t) / sizeof(uint32_t)); ++i) {
    pdw[i] = 0U;
  }

  pdw = (volatile uint32_t *)&sim2.fec[fecn].fec_rmon_r;
  for (i = 0U; i < (sizeof(sim2.fec[fecn].fec_rmon_r) / sizeof(uint32_t)); ++i) {
    pdw[i] = 0U;
  }

  pdw = (volatile uint32_t *)&sim2.fec[fecn].fec_ieee_r;
  for (i = 0U; i < (sizeof(sim2.fec[fecn].fec_ieee_r) / sizeof(uint32_t)); ++i) {
    pdw[i] = 0U;
  }

  sim2.fec[fecn].mibc = 0x00000000U;
}

int NbEthReadInterfaceCounters(const unsigned eth_link_instance,
                               uint32_t *const cntr32_11) {
  const int fecn = NbFecnForEthLinkInstance(eth_link_instance);
  const volatile rmon_rstruct *rx;
  const volatile rmon_tstruct *tx;
  const volatile ieee_rstruct *ieee_rx;
  const volatile ieee_tstruct *ieee_tx;
  uint32_t in_ucast;
  uint32_t out_ucast;

  if (cntr32_11 == NULL) {
    return -1;
  }

  rx = &sim2.fec[fecn].fec_rmon_r;
  tx = &sim2.fec[fecn].fec_rmon_t;
  ieee_rx = &sim2.fec[fecn].fec_ieee_r;
  ieee_tx = &sim2.fec[fecn].fec_ieee_t;

  in_ucast = rx->packets;
  if (in_ucast >= (rx->bc_pkt + rx->mc_pkt)) {
    in_ucast -= (rx->bc_pkt + rx->mc_pkt);
  } else {
    in_ucast = 0U;
  }

  out_ucast = tx->packets;
  if (out_ucast >= (tx->bc_pkt + tx->mc_pkt)) {
    out_ucast -= (tx->bc_pkt + tx->mc_pkt);
  } else {
    out_ucast = 0U;
  }

  cntr32_11[0] = rx->octets;
  cntr32_11[1] = in_ucast;
  cntr32_11[2] = rx->bc_pkt + rx->mc_pkt;
  cntr32_11[3] = ieee_rx->drop;
  cntr32_11[4] =
    rx->crc_align + rx->frag + rx->jab + ieee_rx->crc + ieee_rx->align + ieee_rx->macerr;
  cntr32_11[5] = 0U;
  cntr32_11[6] = tx->octets;
  cntr32_11[7] = out_ucast;
  cntr32_11[8] = tx->bc_pkt + tx->mc_pkt;
  cntr32_11[9] = ieee_tx->drop;
  cntr32_11[10] =
    tx->crc_align + tx->frag + tx->jab + ieee_tx->macerr + ieee_tx->excol + ieee_tx->lcol;

  return 0;
}

int NbEthReadMediaCounters(const unsigned eth_link_instance,
                           uint32_t *const cntr32_12) {
  const int fecn = NbFecnForEthLinkInstance(eth_link_instance);
  const volatile rmon_rstruct *rx;
  const volatile rmon_tstruct *tx;
  const volatile ieee_rstruct *ieee_rx;
  const volatile ieee_tstruct *ieee_tx;

  if (cntr32_12 == NULL) {
    return -1;
  }

  rx = &sim2.fec[fecn].fec_rmon_r;
  tx = &sim2.fec[fecn].fec_rmon_t;
  ieee_rx = &sim2.fec[fecn].fec_ieee_r;
  ieee_tx = &sim2.fec[fecn].fec_ieee_t;

  cntr32_12[0] = ieee_rx->align;
  cntr32_12[1] = ieee_rx->crc;
  cntr32_12[2] = ieee_tx->scol;
  cntr32_12[3] = ieee_tx->mcol;
  cntr32_12[4] = ieee_tx->sqe;
  cntr32_12[5] = ieee_tx->def;
  cntr32_12[6] = ieee_tx->lcol;
  cntr32_12[7] = ieee_tx->excol;
  cntr32_12[8] = ieee_tx->macerr;
  cntr32_12[9] = ieee_tx->cserr;
  cntr32_12[10] = rx->oversize + rx->jab;
  cntr32_12[11] = ieee_rx->macerr;

  return 0;
}

void NbEthClearInterfaceCounters(const unsigned eth_link_instance) {
  NbEthClearFecMibBlocks(NbFecnForEthLinkInstance(eth_link_instance));
}

void NbEthClearMediaCounters(const unsigned eth_link_instance) {
  NbEthClearFecMibBlocks(NbFecnForEthLinkInstance(eth_link_instance));
}

#endif
