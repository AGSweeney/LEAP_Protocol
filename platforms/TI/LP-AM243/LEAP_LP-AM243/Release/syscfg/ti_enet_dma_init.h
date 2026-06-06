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
 * \file ti_enet_open_close.c
 *
 * \brief This file contains enet driver memory allocation related functionality.
 */


#include <drivers/udma/udma_priv.h>


typedef struct EnetApp_TxChInitCfg_s
{
    uint32_t txNumPkts;
    uint32_t useGlobalEvt;
    Enet_Type enetType;
    uint32_t instId;
    uint32_t perMode;
}EnetApp_TxChInitCfg;

typedef struct EnetApp_RxChInitCfg_s
{
    uint32_t rxNumPkts;
    uint32_t useGlobalEvt;
    uint32_t allocMacAddrCnt;
    uint8_t  sizeThreshEn;
    bool     isDefaultFlow;
    Enet_Type enetType;
    uint32_t instId;
    uint32_t chIdx;
}EnetApp_RxChInitCfg;

typedef struct EnetApp_DmaCfg_s
{
    EnetApp_TxChInitCfg txChInitCfg[(2U)];
    EnetApp_RxChInitCfg rxChInitCfg[2U]; 
}EnetApp_DmaCfg;

void EnetApp_updateTxChInitCfg(EnetUdma_OpenTxChPrms * pTxChPrms, uint32_t chId);

void EnetApp_updateRxChInitCfg(EnetUdma_OpenRxFlowPrms * pRxFlowPrms, uint32_t chId);


