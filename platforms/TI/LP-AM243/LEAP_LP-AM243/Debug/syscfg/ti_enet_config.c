/*
 *  Copyright (c) Texas Instruments Incorporated 2024
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
 * \file ti_enet_config.c
 *
 * \brief This file contains enet driver memory allocation related functionality.
 */

/*
 * Enet DMA memory allocation utility functions.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <enet.h>
#include "enet_appmemutils.h"
#include "enet_appmemutils_cfg.h"
#include "enet_apputils.h"
#include <enet_cfg.h>
#include <include/core/enet_per.h>
#include <include/core/enet_utils.h>
#include <include/core/enet_dma.h>
#include <src/dma/udma/enet_udma_memcfg.h>

#include "ti_enet_config.h"

#include <include/per/icssg.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/udma/include/udma_types.h>
#include <drivers/udma/soc/am64x_am243x/udma_soc.h>
#include <networking/enet/hw_include/cslr_icss.h>
#include <networking/enet/hw_include/csl_cpswitch.h>
#include <networking/enet/core/priv/per/icssg_priv.h>
#include <networking/enet/core/src/dma/udma/enet_udma_priv.h>
/* --------------------------------- ICSS-G MACROS --------------------------------- */

#define ENET_MEM_LARGE_POOL_NUM_PKTS        (32U)
#define ENET_MEM_MEDIUM_POOL_NUM_PKTS       (0U)
#define ENET_MEM_SMALL_POOL_NUM_PKTS        (0U)
#define ENET_PKTINFOMEM_ONLY_NUM_PKTS       (0U)


/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */
/* RX flow object memories */
static EnetUdma_RxFlowObjMem gEnetUdmaRxFlowObjMem[ENET_SYSCFG_RX_FLOWS_NUM]
__attribute__ ((aligned(UDMA_CACHELINE_ALIGNMENT), section(".bss:ENET_DMA_OBJ_MEM")));

/* Tx channel object memories */
static EnetUdma_TxChObjMem gEnetUdmaTxChObjMem[ENET_SYSCFG_TX_CHANNELS_NUM]
__attribute__ ((aligned(UDMA_CACHELINE_ALIGNMENT), section(".bss:ENET_DMA_OBJ_MEM")));

#if (UDMA_SOC_CFG_UDMAP_PRESENT == 1)
/* Ring monitor object memories */
static EnetUdma_RingMonObjMem gEnetUdmaRinMonObjMem[ENET_SYSCFG_RING_MON_NUM]
__attribute__ ((aligned(UDMA_CACHELINE_ALIGNMENT), section(".bss:ENET_DMA_OBJ_MEM")));

/* Teardown Cq ring object memories */
static EnetUdma_TdCqRingObjMem gEnetUdmaTdCqRingObjMem[ENET_SYSCFG_TX_CHANNELS_NUM +
                                                       ENET_UDMA_RX_CH_NUM]
__attribute__ ((aligned(UDMA_CACHELINE_ALIGNMENT), section(".bss:ENET_DMA_OBJ_MEM")));
#endif

#if defined (ENET_SOC_HOSTPORT_DMA_TYPE_UDMA)
/* Enet UDMA DESC memories */
static EnetMem_DmaDescMemPoolEntry gDmaDescMemArray[((ENET_SYSCFG_TOTAL_NUM_TX_PKT) +  (ENET_SYSCFG_TOTAL_NUM_RX_PKT))]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_DESC_MEMPOOL")));
static EnetMem_DmaDescMem gDmaDescMemInfoArray[((ENET_SYSCFG_TOTAL_NUM_TX_PKT) +  (ENET_SYSCFG_TOTAL_NUM_RX_PKT))]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_DESC_MEMPOOL")));

/* RX & TX RingAcc memories */
static EnetMem_RingMemPoolEntry gRingMemArray[(ENET_MEM_NUM_RINGS_TYPES *
                                    (ENET_SYSCFG_TX_CHANNELS_NUM + (ENET_SYSCFG_RX_FLOWS_NUM)))]
__attribute__ ((aligned(UDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_RING_MEMPOOL")));

static EnetMem_RingMem gRingMemInfoArray[(ENET_MEM_NUM_RINGS_TYPES *
                                    (ENET_SYSCFG_TX_CHANNELS_NUM + (ENET_SYSCFG_RX_FLOWS_NUM)))]
__attribute__ ((aligned(UDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_RING_MEMPOOL")));

#endif

/* Eth packet info memory Q - Large pool */
static EnetDma_Pkt gAppPktInfoMem_LargePool[ENET_MEM_LARGE_POOL_NUM_PKTS];

/* Eth packet Large pool memories */
static uint8_t gEthPktMem_LargePool[ENET_MEM_LARGE_POOL_NUM_PKTS][ENET_MEM_LARGE_POOL_PKT_SIZE]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_PKT_MEMPOOL")));
static EnetMem_AppPktInfoMem gAppPktInfoContainerMem_LargePool[ENET_MEM_LARGE_POOL_NUM_PKTS];

/* Eth packet info memory Q - Medium pool */
static EnetDma_Pkt gAppPktInfoMem_MediumPool[ENET_MEM_MEDIUM_POOL_NUM_PKTS];

/* Eth packet Medium pool memories */
static uint8_t gEthPktMem_MediumPool[ENET_MEM_MEDIUM_POOL_NUM_PKTS][ENET_MEM_MEDIUM_POOL_PKT_SIZE]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_PKT_MEMPOOL")));
static EnetMem_AppPktInfoMem gAppPktInfoContainerMem_MediumPool[ENET_MEM_MEDIUM_POOL_NUM_PKTS];

/* Eth packet info memory Q - Small pool */
static EnetDma_Pkt gAppPktInfoMem_SmallPool[ENET_MEM_SMALL_POOL_NUM_PKTS];

/* Eth packet Small pool memories */
static uint8_t gEthPktMem_SmallPool[ENET_MEM_SMALL_POOL_NUM_PKTS][ENET_MEM_SMALL_POOL_PKT_SIZE]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_PKT_MEMPOOL")));
static EnetMem_AppPktInfoMem gAppPktInfoContainerMem_SmallPool[ENET_MEM_SMALL_POOL_NUM_PKTS];



static const EnetMem_Cfg gEthMemCfg =
{
    .pktBufPool =
    {

        [ENET_MEM_POOLIDX_LARGE] =
        {
            .pktSize     = ENET_MEM_LARGE_POOL_PKT_SIZE,
            .numPkts     = (ENET_MEM_LARGE_POOL_NUM_PKTS),
            .pktInfoMem  = gAppPktInfoMem_LargePool,
            .pktInfoSize = sizeof(gAppPktInfoMem_LargePool),
            .pktBufMem   = &gEthPktMem_LargePool[0][0],
            .pktBufSize  = sizeof(gEthPktMem_LargePool),
            .pktInfoContainerMem = gAppPktInfoContainerMem_LargePool,
            .pktInfoContainerSize = sizeof(gAppPktInfoContainerMem_LargePool),
        },

        [ENET_MEM_POOLIDX_MEDIUM] =
        {
            .pktSize     = ENET_MEM_MEDIUM_POOL_PKT_SIZE,
            .numPkts     = (ENET_MEM_MEDIUM_POOL_NUM_PKTS),
            .pktInfoMem  = gAppPktInfoMem_MediumPool,
            .pktInfoSize = sizeof(gAppPktInfoMem_MediumPool),
            .pktBufMem   = &gEthPktMem_MediumPool[0][0],
            .pktBufSize  = sizeof(gEthPktMem_MediumPool),
            .pktInfoContainerMem = gAppPktInfoContainerMem_MediumPool,
            .pktInfoContainerSize = sizeof(gAppPktInfoContainerMem_MediumPool),
        },

        [ENET_MEM_POOLIDX_SMALL] =
        {
            .pktSize     = ENET_MEM_SMALL_POOL_PKT_SIZE,
            .numPkts     = (ENET_MEM_SMALL_POOL_NUM_PKTS),
            .pktInfoMem  = gAppPktInfoMem_SmallPool,
            .pktInfoSize = sizeof(gAppPktInfoMem_SmallPool),
            .pktBufMem   = &gEthPktMem_SmallPool[0][0],
            .pktBufSize  = sizeof(gEthPktMem_SmallPool),
            .pktInfoContainerMem = gAppPktInfoContainerMem_SmallPool,
            .pktInfoContainerSize = sizeof(gAppPktInfoContainerMem_SmallPool),
        },

    },
#if defined (ENET_SOC_HOSTPORT_DMA_TYPE_UDMA)
    .ringMem =
    {
        .numRings    =  (ENET_MEM_NUM_RINGS_TYPES *
                                    (ENET_SYSCFG_TX_CHANNELS_NUM + (ENET_SYSCFG_RX_FLOWS_NUM))),
        .ringMemBase =  gRingMemArray,
        .ringMemSize =  sizeof(gRingMemArray),
        .ringInfoContainerBase = gRingMemInfoArray,
        .ringInfoContainerSize = sizeof(gRingMemInfoArray),
    },
    .dmaDescMem =
    {
        .numDesc     = ((ENET_SYSCFG_TOTAL_NUM_TX_PKT) +  (ENET_SYSCFG_TOTAL_NUM_RX_PKT)),
        .descMemBase = gDmaDescMemArray,
        .descMemSize = sizeof(gDmaDescMemArray),
        .descInfoContainerMemBase = gDmaDescMemInfoArray,
        .descInfoContainerMemSize = sizeof(gDmaDescMemInfoArray),
    },
#endif
};

const EnetMem_Cfg * EnetMem_getCfg(void)
{
    return &gEthMemCfg;
}

static const EnetUdma_MemCfg gEthUdmaCfg =
{
    .rxFlowObjMemCfg =
    {
        .numFlows = ENET_SYSCFG_RX_FLOWS_NUM,
        .rxFlowObjMemContainerBase = gEnetUdmaRxFlowObjMem,
        .rxFlowObjMemContainerSize = sizeof(gEnetUdmaRxFlowObjMem),
    },
    .txChObjMemCfg =
    {
        .numCh = ENET_SYSCFG_TX_CHANNELS_NUM,
        .txChObjContainerBase = gEnetUdmaTxChObjMem,
        .txChObjContainerSize = sizeof(gEnetUdmaTxChObjMem),
    },

#if (UDMA_SOC_CFG_UDMAP_PRESENT == 1)
    .ringMonObjMemCfg =
    {
        .numRings = ENET_SYSCFG_RING_MON_NUM,
        .ringMonObjMemContainerBase = gEnetUdmaRinMonObjMem,
        .ringMonObjMemContainerSize = sizeof(gEnetUdmaRinMonObjMem),
    },
    .tdCqRingObjMemCfg =
    {
        .numRings = ENET_SYSCFG_RING_MON_NUM,
        .tdCqRingObjMemContainerBase = gEnetUdmaTdCqRingObjMem,
        .tdCqRingObjMemContainerSize = sizeof(gEnetUdmaTdCqRingObjMem),
    },
#endif
};

const EnetUdma_MemCfg * EnetUdmaMem_getCfg(void)
{
    return &gEthUdmaCfg;
}



/*
 * Enet DMA memory allocation utility functions.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <enet.h>
#include "enet_appmemutils.h"
#include "enet_appmemutils_cfg.h"
#include "enet_apputils.h"
#include <enet_cfg.h>
#include <include/core/enet_per.h>
#include <include/core/enet_utils.h>
#include <include/core/enet_dma.h>
#include <include/per/icssg.h>
#include <priv/per/icssg_priv.h>
#include <soc/k3/icssg_soc.h>

#include <kernel/dpl/TaskP.h>
#include <kernel/dpl/ClockP.h>

#include "ti_enet_config.h"
#include "ti_drivers_config.h"
#include "ti_enet_open_close.h"
#include <enet_appboardutils.h>
#include <enet_appsoc.h>

/* Maximum number of MAC address to be set per CPSW peripheral.
Since CPSW_3G can support upto two MACs, this is set to 2 here */
#define MAC_ADDR_LIST_LEN_SHARED_PER_PER (1U)

#define ENET_MAX_NUM_MAC_ADDR_STORED (5U)

typedef struct EnetApp_MacAddrElem_s
{
    EnetQ_Node node;
    uint8_t macAddr[ENET_MAC_ADDR_LEN];
} EnetApp_MacAddrElem;

typedef struct EnetApp_InstMacInfo_s
{
    Enet_Type enetType;
    uint32_t instId;
    uint32_t addMacAddrCnt;
} EnetApp_InstMacInfo;

typedef struct EnetApp_MacAddrPool_s
{
    bool isInitialized;
    uint32_t macAddrContainerLen;
    EnetApp_MacAddrElem macAddrContainer[ENET_MAX_NUM_MAC_ADDR_STORED];
    EnetQ freeMacAddrQ;
} EnetApp_MacAddrPool;

EnetApp_MacAddrPool gEnetMacAddrPool = { .isInitialized = false };

const EnetApp_InstMacInfo gInstMacAddrInfo []= 
    {
        
        [0] = 
        {
                .enetType       = ENET_ICSSG_DUALMAC,
                .instId         = 2,
                .addMacAddrCnt     = 0,
        },
        [1] = 
        {
                .enetType       = ENET_ICSSG_DUALMAC,
                .instId         = 3,
                .addMacAddrCnt     = 0,
        }
    };

const Icssg_FwPoolMem* EnetCb_GetFwPoolMem(Enet_Type enetType, uint32_t instId)
{
    uint32_t enetIdx = 0;
    const Icssg_FwPoolMem* fwPoolMem;
    extern const  Icssg_FwPoolMem *Icssq_FwPoolMemInfo[2][4];

    switch (enetType)
    {
        case ENET_ICSSG_DUALMAC:
            enetIdx = 0;
            break;

        case ENET_ICSSG_SWITCH:
            enetIdx = 1;
            break;

        default:
            EnetAppUtils_assert(false);
    }
    EnetAppUtils_assert(enetIdx < ENET_ARRAYSIZE(Icssq_FwPoolMemInfo));
    EnetAppUtils_assert(instId < ENET_ARRAYSIZE(Icssq_FwPoolMemInfo[0]));
    fwPoolMem = Icssq_FwPoolMemInfo[enetIdx][instId];
    EnetAppUtils_assert(fwPoolMem != NULL);
    return (fwPoolMem);
}

void EnetApp_getEnetInstInfo(uint32_t enetInstanceId, Enet_Type *enetType, uint32_t *instId)
{
    uint32_t idx = 0;
    const uint32_t instInfoTable[ENET_SYSCFG_MAX_ENET_INSTANCES][3] = { {0, ENET_ICSSG_DUALMAC, 2}, {1, ENET_ICSSG_DUALMAC, 3}, }; 

    EnetAppUtils_assert(enetInstanceId < ENET_SYSCFG_MAX_ENET_INSTANCES);
    for (idx = 0; idx < ENET_SYSCFG_MAX_ENET_INSTANCES; idx++)
    {
        if (instInfoTable[idx][0] ==  enetInstanceId)
        {
            *enetType = (Enet_Type)instInfoTable[idx][1];
            *instId   = instInfoTable[idx][2];
            break;
        }
    }

    /* assert if enetInstanceId is not found in the table. Wrong enetInstanceId parameter */
    EnetAppUtils_assert(idx < ENET_SYSCFG_MAX_ENET_INSTANCES);
}

void EnetApp_getEnetInstMacInfo(Enet_Type enetType,
                             uint32_t instId,
                             Enet_MacPort macPortList[],
                             uint8_t *numMacPorts)
{

    *numMacPorts = 0;
    if (enetType == ENET_ICSSG_DUALMAC && (instId == 2))
    {
        *numMacPorts += 1;
        macPortList[0] = ENET_MAC_PORT_1;
    }
    if (enetType == ENET_ICSSG_DUALMAC && (instId == 3))
    {
        *numMacPorts += 1;
        macPortList[0] = ENET_MAC_PORT_2;
    }
}

void EnetApp_acquireHandleInfo(Enet_Type enetType, uint32_t instId,
                                   EnetApp_HandleInfo *handleInfo)
{
    handleInfo->hEnet = Enet_getHandle(enetType, instId);
    handleInfo->hUdmaDrv = &gUdmaDrvObj[CONFIG_UDMA_PKTDMA_0];
}

void EnetApp_coreAttach(Enet_Type enetType, uint32_t instId,
                            uint32_t coreId,
                            EnetPer_AttachCoreOutArgs *attachInfo)
{
    Enet_IoctlPrms prms;
    int32_t status;
    Enet_Handle hEnet = Enet_getHandle(enetType, instId);

    if (NULL_PTR != attachInfo)
    {
        ENET_IOCTL_SET_INOUT_ARGS(&prms, &coreId, attachInfo);
        ENET_IOCTL(hEnet,
                   coreId,
                   ENET_PER_IOCTL_ATTACH_CORE,
                   &prms,
                   status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("EnetApp_coreAttach failed ENET_PER_IOCTL_ATTACH_CORE: %d\r\n", status);
            EnetAppUtils_assert(false);
        }
    }
    else
    {
        EnetAppUtils_assert(false);
    }
}

void EnetApp_coreDetach(Enet_Type enetType, uint32_t instId,
                            uint32_t coreId,
                            uint32_t coreKey)
{
    Enet_IoctlPrms prms;
    int32_t status;
    Enet_Handle hEnet = Enet_getHandle(enetType, instId);

    ENET_IOCTL_SET_IN_ARGS(&prms, &coreKey);
    ENET_IOCTL(hEnet,
               coreId,
               ENET_PER_IOCTL_DETACH_CORE,
               &prms,
               status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("close() failed ENET_PER_IOCTL_DETACH_CORE: %d\r\n", status);
        EnetAppUtils_assert(false);
    }
}

void EnetApp_releaseHandleInfo(Enet_Type enetType, uint32_t instId)
{
    EnetApp_driverClose(enetType, instId);
}

bool EnetApp_isPortLinked(Enet_Handle hEnet)
{
    Enet_Type enetType;
    uint32_t instId;
    uint32_t linkUpMask = 0U;
    bool linkUp;
    Enet_IoctlPrms prms;
    Enet_MacPort macPortList[ENET_MAC_PORT_NUM];
    uint8_t numMacPorts;
    int32_t status;
    uint32_t i;
    uint32_t coreId = EnetSoc_getCoreId();

    status = Enet_getHandleInfo(hEnet, &enetType, &instId);
    EnetAppUtils_assert(status == ENET_SOK);
    EnetApp_getEnetInstMacInfo(enetType, instId, macPortList, &numMacPorts);
    for (i = 0; i < numMacPorts; i++)
    {
        linkUp = false;
        ENET_IOCTL_SET_INOUT_ARGS(&prms, &macPortList[i], &linkUp);

        ENET_IOCTL(hEnet, coreId, ENET_PER_IOCTL_IS_PORT_LINK_UP, &prms, status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("Failed to get port %u link status: %d\r\n",
                               ENET_MACPORT_ID(macPortList[i]), status);
        }
        linkUpMask |= (linkUp << i);
    }
    
    linkUp =  (linkUpMask != 0) ? true : false;
    return linkUp;
}

int32_t EnetAppSoc_fillMacAddrList(uint8_t macAddr[][ENET_MAC_ADDR_LEN],
                                   uint32_t maxMacEntries,
                                   uint32_t *pAvailMacEntries)
{
    uint32_t numEfusedMacAddrs;
    uint32_t numBoardMacAddrs;
    int32_t status = ENET_SOK;

    if (maxMacEntries >= 1)
    {
        numEfusedMacAddrs = maxMacEntries;
        numBoardMacAddrs = 0;
        EnetSoc_getEFusedMacAddrs(&macAddr[0], &numEfusedMacAddrs);

        if (maxMacEntries > numEfusedMacAddrs)
        {
            EnetBoard_getMacAddrList(&macAddr[numEfusedMacAddrs], (maxMacEntries - numEfusedMacAddrs),&numBoardMacAddrs);
        }
        EnetAppUtils_assert(pAvailMacEntries != NULL);
        *pAvailMacEntries = numEfusedMacAddrs + numBoardMacAddrs;
    }
    else
    {
        status = ENET_EINVALIDPARAMS;
    }

    return status;
}

static void EnetApp_initializeMacAddrPool()
{
    uint8_t macAddrList[ENET_MAX_NUM_MAC_ADDR_STORED][ENET_MAC_ADDR_LEN];
    uint32_t filledMacEntries = 0;

    if (!gEnetMacAddrPool.isInitialized)
    {
        EnetQueue_initQ(&gEnetMacAddrPool.freeMacAddrQ);

                const uint32_t maxMacEntries = ENET_MAX_NUM_MAC_ADDR_STORED;
                EnetAppSoc_fillMacAddrList(macAddrList, maxMacEntries, &filledMacEntries);

        gEnetMacAddrPool.macAddrContainerLen =  filledMacEntries;

        for (uint32_t entryIdx = 0; entryIdx < gEnetMacAddrPool.macAddrContainerLen; entryIdx++)
        {
            EnetApp_MacAddrElem* pElem = &gEnetMacAddrPool.macAddrContainer[entryIdx];
            memcpy(pElem->macAddr, &macAddrList[entryIdx][0], ENET_MAC_ADDR_LEN);
            EnetQueue_enq(&gEnetMacAddrPool.freeMacAddrQ, &pElem->node);
        }
        gEnetMacAddrPool.isInitialized = true;
    }
}

static int32_t EnetApp_getMacAddrFromPool(uint8_t macAddr[ENET_MAC_ADDR_LEN])
{
    int32_t status = ENET_EALLOC;
    EnetApp_MacAddrElem* pElem = (EnetApp_MacAddrElem*)EnetQueue_deq(&gEnetMacAddrPool.freeMacAddrQ);
    if (pElem != NULL)
    {
        memcpy(macAddr, pElem->macAddr, ENET_MAC_ADDR_LEN);
        status = ENET_SOK;
    }
    return status;
}

static int32_t EnetApp_releaseMacAddrToPool(uint8_t macAddr[ENET_MAC_ADDR_LEN])
{
    int32_t status = ENET_EALLOC;
    
    for (uint32_t entryIdx = 0; entryIdx < gEnetMacAddrPool.macAddrContainerLen; entryIdx++)
    {
        EnetApp_MacAddrElem* pElem = &gEnetMacAddrPool.macAddrContainer[entryIdx];
        if (0 == memcmp(pElem->macAddr, &macAddr[0], ENET_MAC_ADDR_LEN))
        {
            EnetQueue_enq(&gEnetMacAddrPool.freeMacAddrQ, &pElem->node);
            status = ENET_SOK;
            break;
        }
    }
    return status;
}

int32_t EnetAppSoc_getMacAddrList(Enet_Type enetType,
                                  uint32_t instId,
                                  uint8_t macAddr[][ENET_MAC_ADDR_LEN],
                                  uint32_t *pAvailMacEntries)
{
    uint32_t sharedListLen = MAC_ADDR_LIST_LEN_SHARED_PER_PER;

    for (uint32_t i = 0U; i < ENET_SYSCFG_MAX_ENET_INSTANCES; i++)
    {
        if ((enetType == gInstMacAddrInfo[i].enetType) && (instId == gInstMacAddrInfo[i].instId))
        {
            sharedListLen += gInstMacAddrInfo[i].addMacAddrCnt;
        }
    }
    EnetApp_initializeMacAddrPool();
    *pAvailMacEntries = 0;
    for (uint32_t idx = 0; idx < sharedListLen; idx++)
    {
        if (EnetApp_getMacAddrFromPool(macAddr[idx]) == ENET_SOK)
        {
            (*pAvailMacEntries)++;
        }
        else
        {
            break;
        }
    }

    return ((*pAvailMacEntries) == 0) ? ENET_EINVALIDPARAMS : ENET_SOK;
}

int32_t EnetAppSoc_releaseMacAddrList(uint8_t macAddr[][ENET_MAC_ADDR_LEN],
                                        uint32_t maxMacEntries)
{
    int32_t status = ENET_SOK;
    for (uint32_t idx = 0; idx < maxMacEntries; idx++)
    {
        status = EnetApp_releaseMacAddrToPool(macAddr[idx]);
        if (status != ENET_SOK)
        {
            break;
        }
    }
    return status;
}

