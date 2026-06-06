/*
 *  Copyright (c) Texas Instruments Incorporated 2025
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!
 * \file ti_dma_init.c
 *
 * \brief This file contains dma obj initialization required by the app.
 */


#include <include/core/enet_dma.h>
#include <drivers/udma/udma_priv.h>
#include <utils/include/enet_apputils.h>
#include "ti_drivers_config.h"
#include "ti_enet_config.h"
#include "ti_enet_dma_init.h"

const EnetApp_DmaCfg g_EnetApp_dmaChParams =
{
    .txChInitCfg = 
    {
        [0] = 
        {
            .txNumPkts    = 8,
            .useGlobalEvt = true,
            .enetType        = ENET_ICSSG_DUALMAC,
            .instId          = 2,
            .perMode         = ENET_UDMA_ICSSG_LRE_TX_OFFLOAD_DISABLE
        },
        [1] = 
        {
            .txNumPkts    = 8,
            .useGlobalEvt = true,
            .enetType        = ENET_ICSSG_DUALMAC,
            .instId          = 3,
            .perMode         = ENET_UDMA_ICSSG_LRE_TX_OFFLOAD_DISABLE
        },
    },
    .rxChInitCfg = 
    {
        [0] = 
        {
            .rxNumPkts       = 8,
            .useGlobalEvt    = true,
            .allocMacAddrCnt = 1,
            .sizeThreshEn    = 7,
            .isDefaultFlow   = false,
            .chIdx           = 0,
            .enetType        = ENET_ICSSG_DUALMAC,
            .instId          = 2,
        },
        [1] = 
        {
            .rxNumPkts       = 8,
            .useGlobalEvt    = true,
            .allocMacAddrCnt = 1,
            .sizeThreshEn    = 7,
            .isDefaultFlow   = false,
            .chIdx           = 0,
            .enetType        = ENET_ICSSG_DUALMAC,
            .instId          = 3,
        },
    }
};

EnetDma_Cfg gDmaCfg =
{
    .hUdmaDrv = &gUdmaDrvObj[CONFIG_UDMA_PKTDMA_0],
    .rxChInitPrms =
    {
        .dmaPriority = UDMA_DEFAULT_RX_CH_DMA_PRIORITY,
    },
};

void EnetApp_updateTxChInitCfg(EnetUdma_OpenTxChPrms * pTxChPrms, uint32_t chId)
{
    EnetAppUtils_assert(chId < ENET_ARRAYSIZE(g_EnetApp_dmaChParams.txChInitCfg));
    pTxChPrms->hUdmaDrv            = gDmaCfg.hUdmaDrv;
    pTxChPrms->disableCacheOpsFlag = false;
    pTxChPrms->numTxPkts           = g_EnetApp_dmaChParams.txChInitCfg[chId].txNumPkts;
    pTxChPrms->useGlobalEvt        = g_EnetApp_dmaChParams.txChInitCfg[chId].useGlobalEvt;

    return ;
}

void EnetApp_updateRxChInitCfg(EnetUdma_OpenRxFlowPrms * pRxFlowPrms, uint32_t chId)
{
    EnetAppUtils_assert(chId < ENET_ARRAYSIZE(g_EnetApp_dmaChParams.rxChInitCfg));
    pRxFlowPrms->numRxPkts             = g_EnetApp_dmaChParams.rxChInitCfg[chId].rxNumPkts;
    pRxFlowPrms->hUdmaDrv              = gDmaCfg.hUdmaDrv;
    pRxFlowPrms->useGlobalEvt          = g_EnetApp_dmaChParams.rxChInitCfg[chId].useGlobalEvt;
    pRxFlowPrms->flowPrms.sizeThreshEn = g_EnetApp_dmaChParams.rxChInitCfg[chId].sizeThreshEn;
    pRxFlowPrms->disableCacheOpsFlag   = false;
    pRxFlowPrms->rxFlowMtu             = ENET_MEM_LARGE_POOL_PKT_SIZE;

        //TODO: take care of useRingMon param while using UDMAP
#if (UDMA_SOC_CFG_UDMAP_PRESENT == 1)
    {
        /* Use ring monitor for the CQ ring of RX flow */
        EnetUdma_UdmaRingPrms *pFqRingPrms = &pRxFlowPrms->udmaChPrms.fqRingPrms;
        pFqRingPrms->useRingMon = rxCfg->useRingMon;
        pFqRingPrms->ringMonCfg.mode = TISCI_MSG_VALUE_RM_MON_MODE_THRESHOLD;
        /* Ring mon low threshold */

#if defined _DEBUG_
        /* In debug mode as CPU is processing lesser packets per event, keep threshold more */
        pFqRingPrms->ringMonCfg.data0 = (pRxFlowPrms->numRxPkts - 10U);
#else
        pFqRingPrms->ringMonCfg.data0 = (pRxFlowPrms->numRxPkts - 20U);
#endif
        /* Ring mon high threshold - to get only low  threshold event, setting high threshold as more than ring depth*/
        pFqRingPrms->ringMonCfg.data1 = pRxFlowPrms->numRxPkts;
    }
#endif

    return ;
}


