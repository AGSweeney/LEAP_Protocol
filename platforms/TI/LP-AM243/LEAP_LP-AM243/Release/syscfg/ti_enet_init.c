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


#include <include/per/icssg.h>
#include <include/mod/icssg_timesync.h>
#include <include/core/enet_dma.h>
#include "ti_drivers_config.h"
#include <drivers/udma/udma_priv.h>
#include <utils/include/enet_apputils.h>
#include <include/core/enet_rm.h>
#include <utils/include/enet_appsoc.h>
#include "ti_enet_config.h"

extern EnetDma_Cfg gDmaCfg;

const EnetRm_ResCfg gEnetRmResCfg =
{
    .selfCoreId = CSL_CORE_ID_R5FSS0_0,
    .resPartInfo =
    {
        .numCores = 1,
        .coreResInfo =
        {
                [0] =
                {
                    .numTxCh       = ENET_SYSCFG_TX_CHANNELS_NUM,
                    .numRxCh       = 1,
                    .numRxFlows    = ENET_SYSCFG_RX_FLOWS_NUM,
                    .coreId        = CSL_CORE_ID_R5FSS0_0,
                    .numMacAddress = 1,
                    .numHwPush     = 0,
                },
        },
        .isStaticTxChanAllocated = false,
    },
    .ioctlPermissionInfo =
    {
        .defaultPermittedCoreMask = 0xFFFFFFFFU,
        .numEntries = 0,
        .entry = {{0}}
    },
    .macList =
    {
        .numMacAddress = 0,
        .macAddress =
        {
                [0] = {0, 0, 0, 0, 0, 0},
                [1] = {0, 0, 0, 0, 0, 0},
                [2] = {0, 0, 0, 0, 0, 0},
                [3] = {0, 0, 0, 0, 0, 0},
                [4] = {0, 0, 0, 0, 0, 0},
                [5] = {0, 0, 0, 0, 0, 0},
                [6] = {0, 0, 0, 0, 0, 0},
                [7] = {0, 0, 0, 0, 0, 0},
                [8] = {0, 0, 0, 0, 0, 0},
                [9] = {0, 0, 0, 0, 0, 0},
        }
    },
};

Icssg_Cfg gEnetIcssgCfg =
{
    .agingPeriod = (uint64_t)ICSSG_CFG_DEFAULT_AGING_PERIOD_MS,
    .vlanCfg =
    {
        .portPri = 0U,
        .portCfi = 0U,
        .portVID = 0U,
    },
    .dmaCfg = &gDmaCfg,
    .resCfg = gEnetRmResCfg,
    .mdioCfg =
    {
        .mode               = MDIO_MODE_MANUAL,
        .mdioBusFreqHz      = 2200000,
        .phyStatePollFreqHz = 22000,
        .pollEnMask         = 0,
        .c45EnMask          = 0,
        .isMaster           = true,
        .disableStateMachineOnInit = true,
    },
    .timeSyncCfg =
    {
        .enable = false,
        .clkType = ICSSG_TIMESYNC_CLKTYPE_WORKING_CLOCK,
        .syncOut_start_WC = 10000,
        .syncOut_pwidth_WC = 25000,
    },
    .mii =
    {
        .layerType = ENET_MAC_LAYER_GMII,
        .sublayerType = ENET_MAC_SUBLAYER_REDUCED,
        .variantType = ENET_MAC_VARIANT_FORCED,
    },
    .cycleTimeNs = ICSSG_IEP_DFLT_CYCLE_TIME_NSECS,
    .mdioLinkIntCfg =
    {
        .mdioLinkStateChangeCb = NULL,
        .mdioLinkStateChangeCbArg = NULL,
    },
    .portLinkIntCfg =
    {
        .portLinkStateChangeCb = NULL,
        .portLinkStateChangeCbArg = NULL,
    },
    .disablePhyDriver = false,
    .qosLevels = 3U,
    .isPremQueEnable = false,
    .clockTypeFw = ICSSG_TIMESYNC_CLKTYPE_WORKING_CLOCK,
};

Icssg_Cfg * EnetApp_getIcssgCfg(const Enet_Type enetType, const uint32_t instId)
{
    Icssg_Cfg * pIcssgCfg = NULL;

    if(Enet_isIcssFamily(enetType))
    {
        pIcssgCfg = &gEnetIcssgCfg;
    }

    return pIcssgCfg;
}

void EnetApp_icssgInitMacAddr(const Enet_Type enetType,
                              const uint32_t instId)
{
    int32_t status;
    Icssg_Cfg *pIcssgCfg = NULL;
    EnetRm_ResCfg *resCfg =NULL;
    pIcssgCfg = EnetApp_getIcssgCfg(enetType, instId);

    EnetAppUtils_assert(pIcssgCfg != NULL);
    resCfg = &pIcssgCfg->resCfg;
    EnetAppUtils_assert(resCfg != NULL);

    status = EnetAppSoc_getMacAddrList(enetType,
                                       instId,
                                       resCfg->macList.macAddress,
                                       &resCfg->macList.numMacAddress);
    EnetAppUtils_assert(status == ENET_SOK);
    if (resCfg->macList.numMacAddress > ENET_ARRAYSIZE(resCfg->macList.macAddress))
    {
        EnetAppUtils_print("EnetApp_icssgInitMacAddr: "
                           "Limiting number of mac address entries to resCfg->macList.macAddress size"
                           "Available:%u, LimitedTo: %u",
                           resCfg->macList.numMacAddress,
                           ENET_ARRAYSIZE(resCfg->macList.macAddress));
        resCfg->macList.numMacAddress = ENET_ARRAYSIZE(resCfg->macList.macAddress);
    }

    EnetAppUtils_updatemacResPart(&resCfg->resPartInfo,
                                  resCfg->macList.numMacAddress,
                                  resCfg->selfCoreId);
}


