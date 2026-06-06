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
 * \file ti_enet_open_close.c
 *
 * \brief This file contains enet driver memory allocation related functionality.
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
#include <enet_board.h>
#include <ti_board_config.h>
#include <include/core/enet_per.h>
#include <include/core/enet_utils.h>
#include <include/core/enet_dma.h>
#include <include/common/enet_utils_dflt.h>
#include <include/per/icssg.h>
#include <priv/per/icssg_priv.h>
#include <drivers/udma/udma_priv.h>
#include <soc/k3/icssg_soc.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/TaskP.h>
#include <kernel/dpl/EventP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/QueueP.h>

#include "ti_enet_config.h"
#include "ti_drivers_config.h"
#include "ti_enet_dma_init.h"
#include "ti_enet_open_close.h"
#include <utils/include/enet_appsoc.h>

#define ENETAPP_PHY_STATEHANDLER_TASK_PRIORITY        (7U)
#define ENETAPP_PHY_STATEHANDLER_TASK_STACK     (3 * 1024)
#define AppEventId_ICSSG_PERIODIC_POLL          (1 << 3)


#include <drivers/udma/udma_priv.h>
#include <drivers/udma.h>

typedef struct EnetAppTxDmaSysCfg_Obj_s
{
    /* TX channel handle */
    EnetDma_TxChHandle hTxCh;
    /* TX channel number */
    uint32_t txChNum;
} EnetAppTxDmaSysCfg_Obj;

typedef struct txAppHandleInfo_s
{
    uint32_t useGlobalEvt;
    
    uint32_t packetsCount;
    
    Enet_Type enetType;
    
    uint32_t instId;
}txAppHandleInfo;

typedef struct EnetAppRxDmaSysCfg_Obj_s
{
    /* RX channel handle */
    EnetDma_RxChHandle hRxCh;
    /* RX flow start index */
    uint32_t rxFlowIdx;
    /* RX flow start index */
    uint32_t rxFlowStartIdx;
    /* num mac Address valid in macAddr[][] list below*/
    uint32_t numValidMacAddress;
    /* MAC address. It's port/ports MAC address in Dual-MAC or
     * host port MAC address in Switch */
    uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN];
} EnetAppRxDmaSysCfg_Obj;

typedef struct EnetAppRxDmaCfg_Info_s
{
    /* mac Address valid */
    uint32_t numValidMacAddress;
    /* max numTxPkts for the channel */
    uint32_t maxNumRxPkts;
    /*! Whether to use the shared global event or not. If set to false, a dedicated event
     *  will be used for this channel. */
    bool  useGlobalEvt;
    /*! Whether to use the shared global event or not. If set to false, a dedicated event
     *  will be used for this channel. */
    bool  useDefaultFlow;
    /*! [IN] UDMAP receive flow packet size based free buffer queue enable configuration
     * to be programmed into the rx_size_thresh_en field of the RFLOW_RFC register.
     * See the UDMAP section of the TRM for more information on this setting.
     * Configuration of the optional size thresholds when this configuration is
     * enabled is done by sending the @ref tisci_msg_rm_udmap_flow_size_thresh_cfg_req
     * message to System Firmware for the receive flow allocated by this request.
     * This parameter can be no greater than
     * @ref TISCI_MSG_VALUE_RM_UDMAP_RX_FLOW_SIZE_THRESH_MAX */
    uint8_t                 sizeThreshEn;
    
    /* Channel id associated with the rx flow. Relevant only for ICSSG as for CPSW rx chIdx is
     * always 0
     */
    uint32_t chIdx;
        
    Enet_Type enetType;
    
    uint32_t instId;
#if (UDMA_SOC_CFG_UDMAP_PRESENT == 1)
    bool  useRingMon;
#endif
} EnetAppRxDmaCfg_Info;

typedef struct EnetAppTxDmaCfg_Info_s
{
    /*! Whether to use the shared global event or not. If set to false, a dedicated event
     *  will be used for this channel. */
    bool  useGlobalEvt;
    /*! UDMA driver handle*/
    Udma_DrvHandle hUdmaDrv;
    
    Enet_Type enetType;
    
    uint32_t instId;
    
    uint32_t txNumPkts;

    /*! Member to pass mode/state value which we use for handling hsr/prp related handling 
     * while sending out data from enet udma driver. */
    uint32_t perMode;
} EnetAppTxDmaCfg_Info;

static void EnetAppUtils_absFlowIdx2FlowIdxOffset(Enet_Handle hEnet,
                                                  uint32_t coreId,
                                                  uint32_t absRxFlowId,
                                                  uint32_t *pStartFlowIdx,
                                                  uint32_t *pFlowIdxOffset);

static void EnetAppUtils_openRxFlowForChIdx(Enet_Type enetType,
                                            Enet_Handle hEnet,
                                            uint32_t coreKey,
                                            uint32_t coreId,
                                            bool useDfltFlow,
                                            uint32_t allocMacAddrCnt,
                                            uint32_t chIdx,
                                            uint32_t *pRxFlowStartIdx,
                                            uint32_t *pRxFlowIdx,
                                            uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN],
                                            EnetDma_RxChHandle *pRxFlowHandle,
                                            EnetUdma_OpenRxFlowPrms *pRxFlowPrms);

static void EnetAppUtils_closeRxFlowForChIdx(Enet_Type enetType,
                                            Enet_Handle hEnet,
                                            uint32_t coreKey,
                                            uint32_t coreId,
                                            bool useDfltFlow,
                                            EnetDma_PktQ *pFqPktInfoQ,
                                            EnetDma_PktQ *pCqPktInfoQ,
                                            uint32_t chIdx,
                                            uint32_t rxFlowStartIdx,
                                            uint32_t rxFlowIdx,
                                            uint32_t numValidMacAddress,
                                            uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN],
                                            EnetDma_RxChHandle hRxFlow);

static void EnetApp_openRxDma(EnetAppRxDmaSysCfg_Obj *rx,
                              Enet_Handle hEnet, 
                              uint32_t coreKey,
                              uint32_t coreId,
                              uint32_t chIdx,
                              Udma_DrvHandle hUdmaDrv,
                              const EnetAppRxDmaCfg_Info *rxCfg);

static void EnetApp_openTxDma(EnetAppTxDmaSysCfg_Obj *tx,
                              uint32_t numTxPkts,
                              Enet_Handle hEnet, 
                              uint32_t coreKey,
                              uint32_t coreId,
                              EnetAppTxDmaCfg_Info *txCfg);

Udma_DrvHandle EnetApp_getUdmaInstanceHandle(void);


typedef struct EnetAppDmaSysCfg_Obj_s
{
    EnetAppTxDmaSysCfg_Obj tx[ENET_SYSCFG_TX_CHANNELS_NUM];
    EnetAppRxDmaSysCfg_Obj rx[ENET_SYSCFG_RX_FLOWS_NUM];
} EnetAppDmaSysCfg_Obj;

typedef struct EnetAppSysCfg_Obj_s
{

    Enet_Handle hEnet[ENET_SYSCFG_NUM_PERIPHERAL];
    EnetAppDmaSysCfg_Obj dma[1];
    ClockP_Object timerObj;

    TaskP_Object task_phyStateHandlerObj;

    SemaphoreP_Object timerSemObj;

    volatile bool timerTaskShutDownFlag;

    volatile bool timerTaskShutDownDoneFlag;

    uint8_t appPhyStateHandlerTaskStack[ENETAPP_PHY_STATEHANDLER_TASK_STACK] __attribute__ ((aligned(32)));
}EnetAppSysCfg_Obj;

extern const EnetApp_DmaCfg g_EnetApp_dmaChParams;

static EnetAppSysCfg_Obj gEnetAppSysCfgObj;

static void EnetApp_txPktNotifyCb(void *cbArg);

static void EnetApp_rxPktNotifyCb(void *cbArg);

static int32_t EnetApp_enablePorts(Enet_Handle hEnet,
                                   Enet_Type enetType,
                                   uint32_t instId,
                                   uint32_t coreId,
                                   Enet_MacPort macPortList[ENET_MAC_PORT_NUM],
                                   uint8_t numMacPorts);

static void EnetApp_getIcssgInitCfg(Enet_Type enetType,
                                    uint32_t instId,
                                    Icssg_Cfg *pIcssgCfg);

void EnetApp_getMacPortLinkCfg(Enet_Type enetType, uint32_t instId, EnetMacPort_LinkCfg *pMacPortLinkCfg, const Enet_MacPort portIdx);


static void EnetApp_phyStateHandler(void * appHandle);

const EnetAppInstInfo gInstInfo []=
    {
        
        [0] = 
        {
                .enetType       = ENET_ICSSG_DUALMAC,
                .instId         = 2,
                .rgmiiEn        = true,
                .startRxChId    = CONFIG_ENET_ICSS0_RX_CH_START,
                .startTxChId    = CONFIG_ENET_ICSS0_TX_CH_START,
                .rxChCount      = CONFIG_ENET_ICSS0_RX_CH_COUNT,
                .txChCount      = CONFIG_ENET_ICSS0_TX_CH_COUNT,
        },
        [1] = 
        {
                .enetType       = ENET_ICSSG_DUALMAC,
                .instId         = 3,
                .rgmiiEn        = true,
                .startRxChId    = CONFIG_ENET_ICSS1_RX_CH_START,
                .startTxChId    = CONFIG_ENET_ICSS1_TX_CH_START,
                .rxChCount      = CONFIG_ENET_ICSS1_RX_CH_COUNT,
                .txChCount      = CONFIG_ENET_ICSS1_TX_CH_COUNT,
        }
    };

static uint32_t EnetApp_getEnetIdx(Enet_Type enetType, uint32_t instId)
{
    uint32_t i, index = 0;

    for (i = 0; i < ENET_SYSCFG_MAX_ENET_INSTANCES; i++)
    {
        if ((instId == gInstInfo[i].instId) && (enetType == gInstInfo[i].enetType))
        {
            index = i;
            break;
        }
    }
    EnetAppUtils_assert(index < ENET_SYSCFG_MAX_ENET_INSTANCES);
    return index;
}

static Enet_Handle EnetApp_doIcssgOpen(Enet_Type enetType, uint32_t instId, const Icssg_Cfg *icssgCfg)
{
    void *perCfg = NULL_PTR;
    uint32_t cfgSize;
    Enet_Handle hEnet;

    EnetAppUtils_assert(true == Enet_isIcssFamily(enetType));

    perCfg = (void *)icssgCfg;
    cfgSize = sizeof(*icssgCfg);

    hEnet = Enet_open(enetType, instId, perCfg, cfgSize);
    if(hEnet == NULL_PTR)
    {
        EnetAppUtils_print("Enet_open failed\r\n");
        EnetAppUtils_assert(hEnet != NULL_PTR);
    }

    return hEnet;
}

static void EnetApp_timerCb(ClockP_Object *clkInst, void * arg)
{
    SemaphoreP_Object *pTimerSem = (SemaphoreP_Object *)arg;

    /* Tick! */
    SemaphoreP_post(pTimerSem);
}
static void EnetApp_phyStateHandler(void * appHandle)
{
    SemaphoreP_Object *timerSem;
    EnetAppSysCfg_Obj *hEnetAppObj       = (EnetAppSysCfg_Obj *)appHandle;

    timerSem = &hEnetAppObj->timerSemObj;
    hEnetAppObj->timerTaskShutDownDoneFlag = false;
    while (hEnetAppObj->timerTaskShutDownFlag != true)
    {
        SemaphoreP_pend(timerSem, SystemP_WAIT_FOREVER);
        /* Enet_periodicTick should be called from only task context */
        for  (uint32_t idx = 0; idx < ENET_SYSCFG_NUM_PERIPHERAL; idx++)
        {
            if (hEnetAppObj->hEnet[idx] != NULL)
            {
                Enet_periodicTick(hEnetAppObj->hEnet[idx]);
            }
        }
    }
    hEnetAppObj->timerTaskShutDownDoneFlag = true;
    TaskP_destruct(&hEnetAppObj->task_phyStateHandlerObj);
    TaskP_exit();
}

static int32_t EnetApp_createPhyStateHandlerTask(EnetAppSysCfg_Obj *hEnetAppObj)
{
    TaskP_Params tskParams;
    int32_t status;

    status = SemaphoreP_constructCounting(&hEnetAppObj->timerSemObj, 0, 128);
    EnetAppUtils_assert(status == SystemP_SUCCESS);
    {
        ClockP_Params clkParams;
        const uint32_t timPeriodTicks = ClockP_usecToTicks((ENETPHY_FSM_TICK_PERIOD_MS)*1000U);  // Set timer expiry time in OS ticks

        ClockP_Params_init(&clkParams);
        clkParams.start     = TRUE;
        clkParams.timeout   = timPeriodTicks;
        clkParams.period    = timPeriodTicks;
        clkParams.args      = &hEnetAppObj->timerSemObj;
        clkParams.callback  = &EnetApp_timerCb;

        /* Creating timer and setting timer callback function*/
        status = ClockP_construct(&hEnetAppObj->timerObj ,
                                  &clkParams);
        if (status == SystemP_SUCCESS)
        {
            hEnetAppObj->timerTaskShutDownFlag = false;
        }
        else
        {
            EnetAppUtils_print("EnetApp_createClock() failed to create clock\r\n");
        }
    }
    /* Initialize the taskperiodicTick params. Set the task priority higher than the
     * default priority (1) */
    TaskP_Params_init(&tskParams);
    tskParams.priority       = ENETAPP_PHY_STATEHANDLER_TASK_PRIORITY;
    tskParams.stack          = &hEnetAppObj->appPhyStateHandlerTaskStack[0];
    tskParams.stackSize      = sizeof(hEnetAppObj->appPhyStateHandlerTaskStack);
    tskParams.args           = hEnetAppObj;
    tskParams.name           = "EnetApp_PhyStateHandlerTask";
    tskParams.taskMain       =  &EnetApp_phyStateHandler;

    status = TaskP_construct(&hEnetAppObj->task_phyStateHandlerObj, &tskParams);
    EnetAppUtils_assert(status == SystemP_SUCCESS);

    return status;

}

static void EnetApp_ConfigureDscpMapping(Enet_Type enetType,
                                         uint32_t instId)
{
    Enet_IoctlPrms prms;
    Enet_Handle hEnet;
    EnetMacPort_SetIngressDscpPriorityMapInArgs inArgs;
    int32_t status;
    uint8_t numMacPorts;
    Enet_MacPort macPortList[ENET_SYSCFG_MAX_MAC_PORTS];
    uint32_t i;

    EnetApp_getEnetInstMacInfo(enetType,
                               instId,
                               macPortList,
                               &numMacPorts);

    hEnet = Enet_getHandle(enetType, instId);
    /*Configure FT3 filter and classifier and Enable DSCP*/
    memset(&inArgs, 0, sizeof(inArgs));
    /* mapped most used 8 dscp priority points to 8 qos levels remaining are routed to 0
       Write a non zero value to required dscp from 0 to 63 in increasing priority order
    */
    inArgs.dscpPriorityMap.tosMap[0]  = 0U;
    inArgs.dscpPriorityMap.tosMap[10] = 1U;
    inArgs.dscpPriorityMap.tosMap[18] = 2U;
    inArgs.dscpPriorityMap.tosMap[26] = 3U;
    inArgs.dscpPriorityMap.tosMap[34] = 4U;
    inArgs.dscpPriorityMap.tosMap[46] = 5U;
    inArgs.dscpPriorityMap.tosMap[48] = 6U;
    inArgs.dscpPriorityMap.tosMap[56] = 7U;

    inArgs.dscpPriorityMap.dscpIPv4En = 1;
    for (i = 0; i < numMacPorts; i++)
    {
        inArgs.macPort = macPortList[i];

        ENET_IOCTL_SET_IN_ARGS(&prms, &inArgs);
        ENET_IOCTL(hEnet, EnetSoc_getCoreId(), ENET_MACPORT_IOCTL_SET_INGRESS_DSCP_PRI_MAP, &prms, status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("Failed to set dscp Priority map for Port %d - %d \r\n", macPortList[i], status);
        }
    }
}

void EnetApp_driverInit()
{
/* keep this implementation that is generic across enetType and instId.
 * Initialization should be done only once.
 */
    int32_t status = ENET_SOK;
    EnetUtils_Cfg utilsPrms;

    /* Initialize Enet driver with default utils */
    Enet_initUtilsCfg(&utilsPrms);
    Enet_init(&utilsPrms);

    status = EnetMem_init();
    EnetAppUtils_assert(ENET_SOK == status);
}

void EnetApp_driverDeInit()
{
/* keep this implementation that is generic across enetType and instId.
 * Denitialization should be done only once.
 */

    EnetMem_deInit();
    Enet_deinit();
}

int32_t EnetApp_driverOpen(Enet_Type enetType, uint32_t instId)
{
    int32_t status = ENET_SOK;

    Icssg_Cfg * const pIcssgCfg = EnetApp_getIcssgCfg(enetType, instId);
    EnetAppUtils_assert(pIcssgCfg != NULL);

    uint32_t numMacPorts = 0;
    Enet_MacPort macPortList[ENET_MAC_PORT_NUM];
    const uint32_t selfCoreId = EnetSoc_getCoreId();
    EnetPer_AttachCoreOutArgs attachInfo;
    EnetAppUtils_assert(Enet_isIcssFamily(enetType) == true);

    if (enetType == ENET_ICSSG_DUALMAC && (instId == 2))
    {
        numMacPorts = 1;
        macPortList[0] = ENET_MAC_PORT_1;
    }
    if (enetType == ENET_ICSSG_DUALMAC && (instId == 3))
    {
        numMacPorts = 1;
        macPortList[0] = ENET_MAC_PORT_2;
    }
    const uint32_t hEnetIndex = EnetApp_getEnetIdx(enetType, instId);

    EnetApp_icssgInitMacAddr(enetType, instId);
    EnetApp_updateIcssgInitCfg(enetType, instId, pIcssgCfg);

    gEnetAppSysCfgObj.hEnet[hEnetIndex] = Enet_open(enetType, instId, (void *)pIcssgCfg, sizeof(*pIcssgCfg));
    if(gEnetAppSysCfgObj.hEnet[hEnetIndex] == NULL_PTR)
    {
        EnetAppUtils_print("Enet_open failed\r\n");
        return ENET_EFAIL;
    }
    
    EnetApp_enablePorts(gEnetAppSysCfgObj.hEnet[hEnetIndex], enetType, instId, selfCoreId, macPortList, numMacPorts);

    /* Enabling DSCP handler will overwrite the PCP filters and
     * will direct all untagged packets to 0th Rx flow.
     * Enable DSCP only when handling Layer2 packets doesn't need QoS support. */
    //EnetApp_ConfigureDscpMapping(enetType, instId);

    if (hEnetIndex == 0)
    {
        status = EnetApp_createPhyStateHandlerTask(&gEnetAppSysCfgObj);
    }
    EnetAppUtils_assert(status == SystemP_SUCCESS);
    /* Open all DMA channels */
    EnetApp_coreAttach(enetType, 
                       instId,
                       selfCoreId,
                       &attachInfo);

    for (uint32_t chIdx = 0; chIdx < ENET_SYSCFG_TX_CHANNELS_NUM; chIdx++)
    {
        EnetUdma_OpenTxChPrms enetTxChCfg;
        EnetAppTxDmaSysCfg_Obj * tx;
        int32_t status;
        Enet_Type enetType;
        uint32_t instId;
        const uint32_t txChEnetType = g_EnetApp_dmaChParams.txChInitCfg[chIdx].enetType;
        const uint32_t txChInstId = g_EnetApp_dmaChParams.txChInitCfg[chIdx].instId;

        status = Enet_getHandleInfo(gEnetAppSysCfgObj.hEnet[hEnetIndex],
                                    &enetType,
                                    &instId);
        EnetAppUtils_assert(status == ENET_SOK);
        (void)instId; /* Instd id not used */

        tx = &gEnetAppSysCfgObj.dma[0].tx[chIdx];

        if(enetType == txChEnetType && instId == txChInstId)
        {
            EnetUdma_initTxChParams(&enetTxChCfg);
            EnetApp_updateTxChInitCfg(&enetTxChCfg, chIdx);

            enetTxChCfg.cbArg     = tx;
            enetTxChCfg.notifyCb  = EnetApp_txPktNotifyCb;
            enetTxChCfg.perMode   = g_EnetApp_dmaChParams.txChInitCfg[chIdx].perMode;


            EnetAppUtils_openTxCh(gEnetAppSysCfgObj.hEnet[hEnetIndex],
                                attachInfo.coreKey,
                                selfCoreId,
                                &tx->txChNum,
                                &tx->hTxCh,
                                &enetTxChCfg);
        }
    }

    for (uint32_t flowIdx = 0; flowIdx<ENET_SYSCFG_RX_FLOWS_NUM; flowIdx++)
    {
        EnetUdma_OpenRxFlowPrms enetRxFlowCfg;
        EnetAppRxDmaSysCfg_Obj * rx;
        int32_t status;
        Enet_Type enetType;
        uint32_t instId;
        const uint32_t allocMacAddrCnt = g_EnetApp_dmaChParams.rxChInitCfg[flowIdx].allocMacAddrCnt;
        const uint32_t isDefaultFlow   = g_EnetApp_dmaChParams.rxChInitCfg[flowIdx].isDefaultFlow;
        const uint32_t rxChEnetType    = g_EnetApp_dmaChParams.rxChInitCfg[flowIdx].enetType;
        const uint32_t rxChInstId      = g_EnetApp_dmaChParams.rxChInitCfg[flowIdx].instId;
        const uint32_t chIdx           = g_EnetApp_dmaChParams.rxChInitCfg[flowIdx].chIdx;

        status = Enet_getHandleInfo(gEnetAppSysCfgObj.hEnet[hEnetIndex],
                                    &enetType,
                                    &instId);
        EnetAppUtils_assert(status == ENET_SOK);
        (void)instId; /* Instd id not used */

        rx = &gEnetAppSysCfgObj.dma[0].rx[flowIdx];

        if(enetType == rxChEnetType && instId == rxChInstId)
        {
            EnetUdma_initRxFlowParams(&enetRxFlowCfg);

            EnetApp_updateRxChInitCfg(&enetRxFlowCfg, flowIdx);

            enetRxFlowCfg.cbArg     = rx;
            enetRxFlowCfg.notifyCb  = EnetApp_rxPktNotifyCb;

            EnetAppUtils_openRxFlowForChIdx(enetType,
                                        gEnetAppSysCfgObj.hEnet[hEnetIndex],
                                        attachInfo.coreKey,
                                        selfCoreId,
                                        isDefaultFlow,
                                        allocMacAddrCnt,
                                        chIdx,
                                        &rx->rxFlowStartIdx,
                                        &rx->rxFlowIdx,
                                        rx->macAddr,
                                        &rx->hRxCh,
                                        &enetRxFlowCfg);

            rx->numValidMacAddress = allocMacAddrCnt;
        }
    }
    return status;
}

static void EnetApp_txPktNotifyCb(void *cbArg)
{


}

static void EnetApp_rxPktNotifyCb(void *cbArg)
{


}

static void EnetApp_asyncIoctlCb(Enet_Event evt,
                                uint32_t evtNum,
                                void *evtCbArgs,
                                void *arg1,
                                void *arg2)
{
    SemaphoreP_Object *pAsyncSem = (SemaphoreP_Object *)evtCbArgs;
    SemaphoreP_post(pAsyncSem);
}
static void EnetApp_macMode2MacMii(emac_mode macMode, EnetMacPort_Interface *pMii)
{
    switch (macMode)
    {
        case MII:
        {
            pMii->layerType    = ENET_MAC_LAYER_MII;
            pMii->sublayerType = ENET_MAC_SUBLAYER_STANDARD;
            pMii->variantType  = ENET_MAC_VARIANT_NONE;
            break;
        }
        case RMII:
        {
            pMii->layerType    = ENET_MAC_LAYER_MII;
            pMii->sublayerType = ENET_MAC_SUBLAYER_REDUCED;
            pMii->variantType  = ENET_MAC_VARIANT_NONE;
            break;
        }
        case RGMII:
        {
            pMii->layerType    = ENET_MAC_LAYER_GMII;
            pMii->sublayerType = ENET_MAC_SUBLAYER_REDUCED;
            pMii->variantType  = ENET_MAC_VARIANT_FORCED;
            break;
        }
        default:
        {
            EnetAppUtils_print("Invalid MAC mode: %u\r\n", macMode);
            EnetAppUtils_assert(false);
        }
    }
}

static void EnetApp_initLinkArgs(const Enet_Type enetType,
                          const uint32_t instId,
                          const Enet_MacPort macPort,
                          EnetPer_PortLinkCfg *pLinkArgs)
{
    EnetMacPort_LinkCfg *pLinkCfg = &pLinkArgs->linkCfg;
    int32_t status = ENET_SOK;

    EnetAppUtils_print("Open MAC port %u\r\n", ENET_MACPORT_ID(macPort));

    /* Set port link params */
    pLinkArgs->macPort = macPort;
    EnetBoard_getMiiConfig(&pLinkArgs->mii);
    EnetApp_getMacPortLinkCfg(enetType, instId, pLinkCfg, macPort);

    /* Setup board for requested Ethernet port */
    EnetBoard_EthPort ethPort =
    {
       .enetType = enetType,
       .instId   = instId,
       .macPort  = macPort,
       .boardId  = EnetBoard_getId(),
       .mii      = pLinkArgs->mii,
    };
    status = EnetBoard_setupPorts(&ethPort, 1U);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("Failed to setup MAC port %u\r\n", ENET_MACPORT_ID(macPort));
        EnetAppUtils_assert(false);
    }

    EnetPhy_Cfg *pPhyCfg = &pLinkArgs->phyCfg;

    const EnetBoard_PhyCfg* pBoardPhyCfg = EnetBoard_getPhyCfg(&ethPort);
    if (pBoardPhyCfg != NULL)
    {
        pPhyCfg->phyAddr     = pBoardPhyCfg->phyAddr;
        pPhyCfg->isStrapped  = pBoardPhyCfg->isStrapped;
        pPhyCfg->loopbackEn  = false;
        pPhyCfg->skipExtendedCfg = pBoardPhyCfg->skipExtendedCfg;
        pPhyCfg->extendedCfgSize = pBoardPhyCfg->extendedCfgSize;
        memcpy(pPhyCfg->extendedCfg, pBoardPhyCfg->extendedCfg, pPhyCfg->extendedCfgSize);
    }
    else
    {
        EnetAppUtils_print("No PHY configuration found for MAC port %u\r\n", ENET_MACPORT_ID(macPort));
        EnetAppUtils_assert(false);
    }
}


static int32_t EnetApp_enablePorts(Enet_Handle hEnet,
                                   Enet_Type enetType,
                                   uint32_t instId,
                                   uint32_t coreId,
                                   Enet_MacPort macPortList[ENET_MAC_PORT_NUM],
                                   uint8_t numMacPorts)
{
    int32_t status = ENET_SOK;
    Enet_IoctlPrms prms;
    uint8_t i;
    SemaphoreP_Object ayncIoctlSemObj;

    status = SemaphoreP_constructBinary(&ayncIoctlSemObj, 0);
    DebugP_assert(SystemP_SUCCESS == status);

    Enet_registerEventCb(hEnet,
                        ENET_EVT_ASYNC_CMD_RESP,
                        0U,
                        EnetApp_asyncIoctlCb,
                        (void *)&ayncIoctlSemObj);

    for (i = 0U; i < numMacPorts; i++)
    {
        EnetPer_PortLinkCfg linkArgs;
        IcssgMacPort_Cfg icssgMacCfg;
        EnetPhy_initCfg(&linkArgs.phyCfg);

        IcssgMacPort_initCfg(&icssgMacCfg);
        icssgMacCfg.specialFramePrio = 1U;

        linkArgs.macCfg = &icssgMacCfg;
        linkArgs.macPort = macPortList[i];

        EnetApp_initLinkArgs(enetType, instId, macPortList[i], &linkArgs);
        ENET_IOCTL_SET_IN_ARGS(&prms, &linkArgs);
        ENET_IOCTL(hEnet,
                   coreId,
                   ENET_PER_IOCTL_OPEN_PORT_LINK,
                   &prms,
                   status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("EnetApp_enablePorts() failed to open MAC port: %d\r\n", status);
        }

        if (status == ENET_SOK)
        {
            IcssgMacPort_SetPortStateInArgs setPortStateInArgs;

            setPortStateInArgs.macPort   = macPortList[i];
            /*Port state is kept disabled during init similar to PRUICSS firmware, also
              on Link state change to up or Down the port state is set to FORWARD or DISABLED respectively*/
            setPortStateInArgs.portState = ICSSG_PORT_STATE_DISABLED;
            ENET_IOCTL_SET_IN_ARGS(&prms, &setPortStateInArgs);
            prms.outArgs = NULL_PTR;

            ENET_IOCTL(hEnet, coreId, ICSSG_PER_IOCTL_SET_PORT_STATE, &prms, status);
            if (status == ENET_SINPROGRESS)
            {
                /* Wait for asyc ioctl to complete */
                do
                {
                    Enet_poll(hEnet, ENET_EVT_ASYNC_CMD_RESP, NULL, 0U);
                    status = SemaphoreP_pend(&ayncIoctlSemObj, SystemP_WAIT_FOREVER);
                    if (SystemP_SUCCESS == status)
                    {
                        break;
                    }
                } while (1);

                status = ENET_SOK;
            }
            else
            {
                EnetAppUtils_print("Failed to set port state: %d\n", status);
            }
        }
    }

    /* Show alive PHYs */
    if (status == ENET_SOK)
    {
        Enet_IoctlPrms prms;
        bool alive;
        int32_t i;

        for (i = 0U; i < ENET_MDIO_PHY_CNT_MAX; i++)
        {
            ENET_IOCTL_SET_INOUT_ARGS(&prms, &i, &alive);
            ENET_IOCTL(hEnet,
                       coreId,
                       ENET_MDIO_IOCTL_IS_ALIVE,
                       &prms,
                       status);
            if (status == ENET_SOK)
            {
                if (alive == true)
                {
                    EnetAppUtils_print("PHY %d is alive\r\n", i);
                }
            }
            else
            {
                EnetAppUtils_print("Failed to get PHY %d alive status: %d\r\n", i, status);
            }
        }
    }

    SemaphoreP_destruct(&ayncIoctlSemObj);
    Enet_unregisterEventCb(hEnet, ENET_EVT_ASYNC_CMD_RESP, 0U);

    return status;
}

static void EnetApp_deleteClock(EnetAppSysCfg_Obj *hEnetAppObj)
{
    ClockP_stop(&hEnetAppObj->timerObj);
    hEnetAppObj->timerTaskShutDownFlag = true;
    /* Post Timer Sem once to get the Periodic Tick task terminated */
    SemaphoreP_post(&hEnetAppObj->timerSemObj);

    do
    {
        ClockP_usleep(ClockP_ticksToUsec(1));
    } while (hEnetAppObj->timerTaskShutDownDoneFlag != true);

    SemaphoreP_destruct(&hEnetAppObj->timerSemObj);
    ClockP_destruct(&hEnetAppObj->timerObj);
}

void EnetApp_closePortLink(Enet_Type enetType, uint32_t instId)
{
    Enet_IoctlPrms prms;
    int32_t status;
    Enet_MacPort macPortList[ENET_MAC_PORT_NUM];
    uint8_t numMacPorts;
    Enet_Handle hEnet = Enet_getHandle(enetType, instId);
    uint32_t selfCoreId;
    uint32_t i;
    selfCoreId   = EnetSoc_getCoreId();

    EnetApp_getEnetInstMacInfo(enetType, instId, macPortList, &numMacPorts);

    for (i = 0U; i < numMacPorts; i++)
    {
        Enet_MacPort macPort = macPortList[i];

        ENET_IOCTL_SET_IN_ARGS(&prms, &macPort);
        ENET_IOCTL(hEnet,
                   selfCoreId,
                   ENET_PER_IOCTL_CLOSE_PORT_LINK,
                   &prms,
                   status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("close() failed to close MAC port: %d\r\n", status);
        }
    }
}

void EnetApp_driverClose(Enet_Type enetType, uint32_t instId)
{
    Enet_MacPort macPortList[ENET_MAC_PORT_NUM];
    uint8_t numMacPorts;
    Enet_Handle hEnet = Enet_getHandle(enetType, instId);

    EnetAppUtils_assert(Enet_isIcssFamily(enetType) == true);
    EnetApp_getEnetInstMacInfo(enetType, instId, macPortList, &numMacPorts);
    EnetApp_deleteClock(&gEnetAppSysCfgObj);
    Enet_unregisterEventCb(hEnet,
                            ENET_EVT_ASYNC_CMD_RESP,
                            0U);

    EnetAppUtils_print("Unregister TX timestamp callback\r\n");
    Enet_unregisterEventCb(hEnet,
                            ENET_EVT_TIMESTAMP_TX,
                            0U);

    Enet_close(hEnet);
}




static uint32_t EnetApp_retrieveFreeTxPkts(EnetDma_TxChHandle hTxCh, EnetDma_PktQ *txPktInfoQ)
{
    EnetDma_PktQ txFreeQ;
    EnetDma_Pkt *pktInfo;
    uint32_t txFreeQCnt = 0U;
    int32_t status;

    EnetQueue_initQ(&txFreeQ);

    /* Retrieve any packets that may be free now */
    status = EnetDma_retrieveTxPktQ(hTxCh, &txFreeQ);
    if (status == ENET_SOK)
    {
        txFreeQCnt = EnetQueue_getQCount(&txFreeQ);

        pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&txFreeQ);
        while (NULL != pktInfo)
        {
            EnetDma_checkPktState(&pktInfo->pktState,
                                    ENET_PKTSTATE_MODULE_APP,
                                    ENET_PKTSTATE_APP_WITH_DRIVER,
                                    ENET_PKTSTATE_APP_WITH_FREEQ);

            EnetQueue_enq(txPktInfoQ, &pktInfo->node);
            pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&txFreeQ);
        }
    }
    else
    {
        EnetAppUtils_print("retrieveFreeTxPkts() failed to retrieve pkts: %d\r\n", status);
    }

    return txFreeQCnt;
}

int32_t EnetApp_enablePortTsEvent(Enet_Handle hEnet, uint32_t coreId, Enet_MacPort macPort[], uint32_t numPorts)
{
    /* Per MacPort timestamping enabling is not supported in ICSSG */
    /* Need this funtion definition to align CPSW and ICSSG for TSN libs */
    return ENET_SOK;
}

int32_t EnetApp_getRxTimeStamp(Enet_Handle hEnet, uint32_t coreId, EnetTimeSync_GetEthTimestampInArgs* inArgs, uint64_t *ts)
{
    /* Not supported for ICSSG, the dmaPktInfo has the time stamp of the pkt */
    /* Need this funtion definition to align CPSW and ICSSG for TSN libs */
    return ENET_SOK;
}

int32_t EnetApp_filterPriorityPacketsCfg(Enet_Handle hEnet, uint32_t coreId)
{
    /* Need this funtion definition to align CPSW and ICSSG for TSN libs */
    return ENET_SOK;
}

int32_t EnetApp_setTimeStampComplete(Enet_Handle hEnet, uint32_t coreId)
{
    int32_t status = ENET_SOK;
    Enet_IoctlPrms prms;
    EnetUtils_delayNs(1000*1000); // wait for 1ms, assuming the ioctl will complete.
    // TODO: Need to replace by registering proper PRU isr which hits after completion

    /* Pass the completion ioctl, so that get current time stamp ioctl works */
    memset((void *)(&prms), 0, sizeof(prms));
    ENET_IOCTL(hEnet, coreId, ENET_TIMESYNC_IOCTL_SET_TIMESTAMP_COMPLETE, &prms, status);
    return status;
}

static bool IsMacAddrSet(uint8_t *mac)
{
    return ((mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5]) != 0);
}

/* Need this funtion definition to align CPSW and ICSSG for TSN libs */
int32_t EnetApp_applyClassifier(Enet_Handle hEnet, uint32_t coreId, uint8_t *dstMacAddr, uint32_t vlanId,
                                uint32_t ethType, uint32_t rxFlowIdx)
{
    Icssg_FdbEntry fdbEntry;
    Enet_IoctlPrms prms;
    int32_t status = ENET_SOK;
    SemaphoreP_Object asyncIoctlSemObj;

    EnetAppUtils_print("For ICSSG, EthType and VlanId are not used to match the packet only dest addr is used \r\n");
    if(IsMacAddrSet(dstMacAddr) == false)
    {
        return status;
    }
    status = SemaphoreP_constructBinary(&asyncIoctlSemObj, 0);
    DebugP_assert(SystemP_SUCCESS == status);

    Enet_registerEventCb(hEnet,
                        ENET_EVT_ASYNC_CMD_RESP,
                        0U,
                        EnetApp_asyncIoctlCb,
                        (void *)&asyncIoctlSemObj);

    memset(&fdbEntry, 0, sizeof(fdbEntry));
    fdbEntry.vlanId = ((int16_t)-1);
    memcpy(&fdbEntry.macAddr, dstMacAddr, 6U);

    if(hEnet->enetPer->enetType == ENET_ICSSG_SWITCH)
    {
        fdbEntry.fdbEntry[0] = (uint8_t)((ICSSG_FDB_ENTRY_P0_MEMBERSHIP |
                                         ICSSG_FDB_ENTRY_BLOCK          |
                                         ICSSG_FDB_ENTRY_VALID) & 0xFF);
        fdbEntry.fdbEntry[1] = (uint8_t)((ICSSG_FDB_ENTRY_P0_MEMBERSHIP |
                                         ICSSG_FDB_ENTRY_BLOCK          |
                                         ICSSG_FDB_ENTRY_VALID) & 0xFF);
    }
    else
    {
        fdbEntry.fdbEntry[0] = (uint8_t)((ICSSG_FDB_ENTRY_P1_MEMBERSHIP |
                                         ICSSG_FDB_ENTRY_P2_MEMBERSHIP  |
                                         ICSSG_FDB_ENTRY_BLOCK          |
                                         ICSSG_FDB_ENTRY_VALID) & 0xFF);
        fdbEntry.fdbEntry[1] = (uint8_t)((ICSSG_FDB_ENTRY_P1_MEMBERSHIP |
                                         ICSSG_FDB_ENTRY_P2_MEMBERSHIP  |
                                         ICSSG_FDB_ENTRY_BLOCK          |
                                         ICSSG_FDB_ENTRY_VALID) & 0xFF);
    }

    ENET_IOCTL_SET_IN_ARGS(&prms, &fdbEntry);
    ENET_IOCTL(hEnet, coreId, ICSSG_FDB_IOCTL_ADD_ENTRY, &prms, status);
    if (status == ENET_SINPROGRESS)
    {
        /* Wait for asyc ioctl to complete */
        do
        {
            Enet_poll(hEnet, ENET_EVT_ASYNC_CMD_RESP, NULL, 0U);
            status = SemaphoreP_pend(&asyncIoctlSemObj, SystemP_WAIT_FOREVER);
            if (SystemP_SUCCESS == status)
            {
                break;
            }
        } while (1);

        status = ENET_SOK;
    }
    else
    {
        EnetAppUtils_print("Failed to set the SPL mac entry: %d\n", status);
    }

    IcssgMacPort_ConfigSpecialFramePrioInArgs SplFramePrio;
    SplFramePrio.macPort = ENET_MAC_PORT_1;
    SplFramePrio.specialFramePrio = rxFlowIdx; /* This is queue priority, for now filling channel number */
    ENET_IOCTL_SET_IN_ARGS(&prms, &SplFramePrio);
    ENET_IOCTL(hEnet, coreId, ICSSG_MACPORT_IOCTL_CONFIG_SPL_FRAME_PRIO, &prms, status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("Failed to set the SPL frame priority : %d\n", status);
    }

    /* for second mac port */
    SplFramePrio.macPort = ENET_MAC_PORT_2;
    ENET_IOCTL_SET_IN_ARGS(&prms, &SplFramePrio);
    ENET_IOCTL(hEnet, coreId, ICSSG_MACPORT_IOCTL_CONFIG_SPL_FRAME_PRIO, &prms, status);
    if (status != ENET_SOK)
    {
       EnetAppUtils_print("Failed to set the SPL frame priority : %d\n", status);
    }
    return status;
}




static void EnetAppUtils_openRxFlowForChIdx(Enet_Type enetType,
                                            Enet_Handle hEnet,
                                            uint32_t coreKey,
                                            uint32_t coreId,
                                            bool useDefaultFlow,
                                            uint32_t allocMacAddrCnt,
                                            uint32_t chIdx,
                                            uint32_t *pRxFlowStartIdx,
                                            uint32_t *pRxFlowIdx,
                                            uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN],
                                            EnetDma_RxChHandle *pRxFlowHandle,
                                            EnetUdma_OpenRxFlowPrms *pRxFlowPrms)
{
    EnetDma_Handle hDma = Enet_getDmaHandle(hEnet);
    int32_t status = ENET_SOK;

    EnetAppUtils_assert(hDma != NULL);

    status = EnetAppUtils_allocRxFlowForChIdx(hEnet,
                                              coreKey,
                                              coreId,
                                              chIdx,
                                              pRxFlowStartIdx,
                                              pRxFlowIdx);
    EnetAppUtils_assert(status == ENET_SOK);

    pRxFlowPrms->startIdx = *pRxFlowStartIdx;
    pRxFlowPrms->flowIdx  = *pRxFlowIdx;
    pRxFlowPrms->chIdx    = chIdx;

    *pRxFlowHandle = EnetUdma_openRxFlow(hDma, pRxFlowPrms);
    EnetAppUtils_assert(*pRxFlowHandle != NULL);

     for (uint32_t i = 0; i < allocMacAddrCnt; i++)
    {
        status = EnetAppUtils_allocMac(hEnet, coreKey, coreId, macAddr[i]);
        EnetAppUtils_assert(status == ENET_SOK);
        {
            // Should we add this entry to ICSSG FDB?
        }
    }
    EnetAppUtils_assert(status == ENET_SOK);
}

static void EnetAppUtils_closeRxFlowForChIdx(Enet_Type enetType,
                                            Enet_Handle hEnet,
                                            uint32_t coreKey,
                                            uint32_t coreId,
                                            bool useDefaultFlow,
                                            EnetDma_PktQ *pFqPktInfoQ,
                                            EnetDma_PktQ *pCqPktInfoQ,
                                            uint32_t chIdx,
                                            uint32_t rxFlowStartIdx,
                                            uint32_t rxFlowIdx,
                                            uint32_t allocMacAddrCnt,
                                            uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN],
                                            EnetDma_RxChHandle hRxFlow)
{
    int32_t status = ENET_SOK;

    EnetQueue_initQ(pFqPktInfoQ);
    EnetQueue_initQ(pCqPktInfoQ);

    EnetDma_disableRxEvent(hRxFlow);

    for(uint32_t i = 0; i < allocMacAddrCnt; i++)
    {
        status = EnetAppUtils_freeMac(hEnet,
                                      coreKey,
                                      coreId,
                                      macAddr[i]);
        EnetAppUtils_assert(status == ENET_SOK);
    }

    status = EnetUdma_closeRxFlow(hRxFlow, pFqPktInfoQ, pCqPktInfoQ);
    EnetAppUtils_assert(status == ENET_SOK);


    status = EnetAppUtils_freeRxFlowForChIdx(hEnet,
                                             coreKey,
                                             coreId,
                                             chIdx,
                                             rxFlowIdx);
    EnetAppUtils_assert(status == ENET_SOK);
}

void EnetApp_closeTxDma(uint32_t enetTxDmaChId,
                        Enet_Handle hEnet, 
                        uint32_t coreKey,
                        uint32_t coreId,
                        EnetDma_PktQ *fqPktInfoQ,
                        EnetDma_PktQ *cqPktInfoQ)
{
    EnetAppTxDmaSysCfg_Obj *tx;

    EnetAppUtils_assert(enetTxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma[0U].tx));
    tx = &gEnetAppSysCfgObj.dma[0].tx[enetTxDmaChId];

    EnetQueue_initQ(fqPktInfoQ);
    EnetQueue_initQ(cqPktInfoQ);
    EnetApp_retrieveFreeTxPkts(tx->hTxCh, cqPktInfoQ);
    EnetAppUtils_closeTxCh(hEnet,
                           coreKey,
                           coreId,
                           fqPktInfoQ,
                           cqPktInfoQ,
                           tx->hTxCh,
                           tx->txChNum);
    memset(tx, 0, sizeof(*tx));
}

void EnetApp_closeRxDma(uint32_t enetRxDmaChId,
                        Enet_Handle hEnet, 
                        uint32_t coreKey,
                        uint32_t coreId,
                        EnetDma_PktQ *fqPktInfoQ,
                        EnetDma_PktQ *cqPktInfoQ)
{
    Enet_Type enetType;
    uint32_t instId;
    int32_t status;
    const EnetAppRxDmaCfg_Info rxDmaInfo[ENET_SYSCFG_RX_FLOWS_NUM] = 
    {
        
        [0] = 
        {
                .maxNumRxPkts    = 8,
                .numValidMacAddress = 1,
                .useGlobalEvt    = true,
                .useDefaultFlow  = false,
                .sizeThreshEn    = 7,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 2,
                .chIdx           = 0,
        },
        [1] = 
        {
                .maxNumRxPkts    = 8,
                .numValidMacAddress = 1,
                .useGlobalEvt    = true,
                .useDefaultFlow  = false,
                .sizeThreshEn    = 7,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 3,
                .chIdx           = 0,
        }
    
    };

    status = Enet_getHandleInfo(hEnet,
                                &enetType,
                                &instId);
    EnetAppUtils_assert(status == ENET_SOK);
    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma[0].rx));
    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(rxDmaInfo));

    /* Close RX channel */
    EnetQueue_initQ(fqPktInfoQ);
    EnetQueue_initQ(cqPktInfoQ);

    EnetAppRxDmaSysCfg_Obj *pRx= &gEnetAppSysCfgObj.dma[0].rx[enetRxDmaChId];
    EnetAppUtils_closeRxFlowForChIdx(enetType,
                                         hEnet,
                                         coreKey,
                                         coreId,
                                         rxDmaInfo[enetRxDmaChId].useDefaultFlow,
                                         fqPktInfoQ,
                                         cqPktInfoQ,
                                         rxDmaInfo[enetRxDmaChId].chIdx,
                                         pRx->rxFlowStartIdx,
                                         pRx->rxFlowIdx,
                                         pRx->numValidMacAddress,
                                         pRx->macAddr,
                                         pRx->hRxCh);
    EnetAppSoc_releaseMacAddrList(pRx->macAddr, pRx->numValidMacAddress);
    memset(pRx, 0, sizeof(*pRx));
}

void EnetApp_txChInPeripheral(Enet_Type enetType, uint32_t instId, uint32_t *startIdx, uint32_t *chCount)
{
    uint32_t i, count = 0;
    int32_t startIdOffset = -1;
    const txAppHandleInfo txChInfo[ENET_SYSCFG_TX_CHANNELS_NUM] =
                           {
                               
        [0] = 
        {
                .useGlobalEvt    = true,
                .packetsCount    = 8,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 2,
        },
        [1] = 
        {
                .useGlobalEvt    = true,
                .packetsCount    = 8,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 3,
        }
                           };
    
    for (i = 0; i < ENET_SYSCFG_TX_CHANNELS_NUM; i++)
    {
        if (enetType == txChInfo[i].enetType && instId == txChInfo[i].instId)
        {
            if (startIdOffset == -1)
            {
                startIdOffset = i;
            }
            count++;
        }
    }
    *startIdx = startIdOffset;
    *chCount  = count;
}

void EnetApp_rxChInPeripheral(Enet_Type enetType, uint32_t instId, uint32_t *startIdx, uint32_t *chCount)
{
    uint32_t i, count = 0;
    int32_t startIdOffset = -1;
    const EnetApp_GetRxDmaHandleOutArgs rxChInfo[ENET_SYSCFG_RX_FLOWS_NUM] =
                           {
                               
        [0] = 
        {
                .maxNumRxPkts    = 8,
                .numValidMacAddress = 1,
                .useGlobalEvt    = true,
                .useDefaultFlow  = false,
                .sizeThreshEn    = 7,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 2,
                .chIdx           = 0,
        },
        [1] = 
        {
                .maxNumRxPkts    = 8,
                .numValidMacAddress = 1,
                .useGlobalEvt    = true,
                .useDefaultFlow  = false,
                .sizeThreshEn    = 7,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 3,
                .chIdx           = 0,
        }
                           };
    
    for (i = 0; i < ENET_SYSCFG_RX_FLOWS_NUM; i++)
    {
        if (enetType == rxChInfo[i].enetType && instId == rxChInfo[i].instId)
        {
            if (startIdOffset == -1)
            {
                startIdOffset = i;
            }
            count++;
        }
    }
    *startIdx = startIdOffset;
    *chCount  = count;
}

void EnetApp_getTxDmaHandle(uint32_t enetTxDmaChId,
                            const EnetApp_GetDmaHandleInArgs *inArgs,
                            EnetApp_GetTxDmaHandleOutArgs *outArgs)
{
    int32_t status;
    EnetAppTxDmaSysCfg_Obj *tx;
    const uint32_t module = 0;
    const uint32_t txNumPkts[ENET_SYSCFG_TX_CHANNELS_NUM] = 
                           {
                            8,8
                           };

    const txAppHandleInfo txChInfo[ENET_SYSCFG_TX_CHANNELS_NUM] =
                           {
                               
        [0] = 
        {
                .useGlobalEvt    = true,
                .packetsCount    = 8,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 2,
        },
        [1] = 
        {
                .useGlobalEvt    = true,
                .packetsCount    = 8,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 3,
        }
                           };

    EnetAppUtils_assert(enetTxDmaChId < ENET_ARRAYSIZE(txNumPkts));
    EnetAppUtils_assert(enetTxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma[module].tx));
    tx = &gEnetAppSysCfgObj.dma[module].tx[enetTxDmaChId];

    EnetAppUtils_assert(tx->hTxCh != NULL);
    status = EnetDma_registerTxEventCb(tx->hTxCh, inArgs->notifyCb, inArgs->cbArg);
    EnetAppUtils_assert(status == ENET_SOK);
    outArgs->hTxCh = tx->hTxCh;
    outArgs->txChNum = tx->txChNum;
    outArgs->maxNumTxPkts = txChInfo[enetTxDmaChId].packetsCount;
    outArgs->useGlobalEvt = txChInfo[enetTxDmaChId].useGlobalEvt;
    return;
}

void EnetApp_getMacAddress(uint32_t enetRxDmaChId,
                            EnetApp_GetMacAddrOutArgs *outArgs)
{

    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma[0].rx));
    EnetAppRxDmaSysCfg_Obj* rx = &gEnetAppSysCfgObj.dma[0].rx[enetRxDmaChId]; // 

    outArgs->macAddressCnt = rx->numValidMacAddress;
    EnetAppUtils_assert(outArgs->macAddressCnt <= ENET_MAX_NUM_MAC_PER_PHER);
    for (uint32_t i = 0; i < outArgs->macAddressCnt; i++)
    {
        EnetUtils_copyMacAddr(outArgs->macAddr[i], rx->macAddr[i]);
    }

}

void EnetApp_getNonPtpRxDmaInfo(Enet_Type enetType,
                                uint32_t instId,
                                uint32_t nonPtpRxFlowId[],
                                uint8_t *nonPtpRxFlowNum)
{
    if (enetType == ENET_ICSSG_DUALMAC && (instId == 2U))
    {
        *nonPtpRxFlowNum = 1U;
        nonPtpRxFlowId[0] = ENET_DMA_RX_CH0;
    }
    if (enetType == ENET_ICSSG_DUALMAC && (instId == 3U))
    {
        *nonPtpRxFlowNum = 1U;
        nonPtpRxFlowId[0] = ENET_DMA_RX_CH1;
    }
    if (enetType == ENET_ICSSG_SWITCH && (instId == 1U))
    {
        *nonPtpRxFlowNum = 0U;
    }
    EnetAppUtils_assert(*nonPtpRxFlowNum != 0U);
}

void EnetApp_getNonPtpTxDmaInfo(Enet_Type enetType,
                                uint32_t instId,
                                uint32_t nonPtpTxFlowId[],
                                uint8_t *nonPtpTxFlowNum)
{
    if (enetType == ENET_ICSSG_DUALMAC && (instId == 2U))
    {
        *nonPtpTxFlowNum = 1U;
        nonPtpTxFlowId[0] = ENET_DMA_TX_CH0;
    }
    if (enetType == ENET_ICSSG_DUALMAC && (instId == 3U))
    {
        *nonPtpTxFlowNum = 1U;
        nonPtpTxFlowId[0] = ENET_DMA_TX_CH1;
    }
    if (enetType == ENET_ICSSG_SWITCH && (instId == 1U))
    {
        *nonPtpTxFlowNum = 0U;
    }
    EnetAppUtils_assert(*nonPtpTxFlowNum != 0U);
}
void EnetApp_getRxDmaHandle(uint32_t enetRxDmaChId,
                            const EnetApp_GetDmaHandleInArgs *inArgs,
                            EnetApp_GetRxDmaHandleOutArgs *outArgs)
{
    int32_t status;
    uint32_t startIdOffset;
    EnetAppRxDmaSysCfg_Obj *rx;

    const uint32_t module = 0;
    const EnetApp_GetRxDmaHandleOutArgs rxDmaInfo[ENET_SYSCFG_RX_FLOWS_NUM] = 
    {
        
        [0] = 
        {
                .maxNumRxPkts    = 8,
                .numValidMacAddress = 1,
                .useGlobalEvt    = true,
                .useDefaultFlow  = false,
                .sizeThreshEn    = 7,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 2,
                .chIdx           = 0,
        },
        [1] = 
        {
                .maxNumRxPkts    = 8,
                .numValidMacAddress = 1,
                .useGlobalEvt    = true,
                .useDefaultFlow  = false,
                .sizeThreshEn    = 7,
                .enetType        = ENET_ICSSG_DUALMAC,
                .instId          = 3,
                .chIdx           = 0,
        }
    };

    for (startIdOffset = 0; startIdOffset < ENET_SYSCFG_RX_FLOWS_NUM; startIdOffset++)
    {
        if (inArgs->enetType == rxDmaInfo[startIdOffset].enetType && inArgs->instId == rxDmaInfo[startIdOffset].instId)
        {
            break;
        }
    }
    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma[module].rx));
    rx = &gEnetAppSysCfgObj.dma[module].rx[enetRxDmaChId];

    EnetAppUtils_assert(rx->hRxCh != NULL);
    status = EnetDma_registerRxEventCb(rx->hRxCh, inArgs->notifyCb, inArgs->cbArg);
    EnetAppUtils_assert(status == ENET_SOK);
    
    outArgs->hRxCh = rx->hRxCh;
    outArgs->rxFlowIdx = rx->rxFlowIdx;
    outArgs->rxFlowStartIdx = rx->rxFlowStartIdx;
    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(rxDmaInfo));
    outArgs->numValidMacAddress = rx->numValidMacAddress;
    for (uint32_t i = 0; i < rx->numValidMacAddress; i++)
    {
        EnetUtils_copyMacAddr(outArgs->macAddr[i], rx->macAddr[i]);
    }
    outArgs->maxNumRxPkts   = rxDmaInfo[enetRxDmaChId].maxNumRxPkts;
    outArgs->sizeThreshEn   = rxDmaInfo[enetRxDmaChId].sizeThreshEn;
    outArgs->useDefaultFlow = rxDmaInfo[enetRxDmaChId].useDefaultFlow;
    outArgs->useGlobalEvt   = rxDmaInfo[enetRxDmaChId].useGlobalEvt;
    outArgs->chIdx          = rxDmaInfo[enetRxDmaChId].chIdx;
    return;
}

#define ENET_SYSCFG_DEFAULT_NUM_TX_PKT                                     (16U)
#define ENET_SYSCFG_DEFAULT_NUM_RX_PKT                                     (32U)


void EnetAppUtils_setCommonRxFlowPrms(EnetUdma_OpenRxFlowPrms *pRxFlowPrms)
{
    pRxFlowPrms->numRxPkts           = ENET_SYSCFG_DEFAULT_NUM_RX_PKT;
    pRxFlowPrms->disableCacheOpsFlag = false;
    pRxFlowPrms->rxFlowMtu           = ENET_MEM_LARGE_POOL_PKT_SIZE;
}

void EnetAppUtils_setCommonTxChPrms(EnetUdma_OpenTxChPrms *pTxChPrms)
{
    pTxChPrms->numTxPkts           = ENET_SYSCFG_DEFAULT_NUM_TX_PKT;
    pTxChPrms->disableCacheOpsFlag = false;
}


Udma_DrvHandle EnetApp_getUdmaInstanceHandle(void)
{
    Udma_DrvHandle hUdmaDrv;

    hUdmaDrv = &gUdmaDrvObj[CONFIG_UDMA_PKTDMA_0];
    return hUdmaDrv;
}






static const Mdio_Cfg enetAppIcssgMdioCfg =
{
    .mode               = MDIO_MODE_MANUAL,
    .mdioBusFreqHz      = 2200000,
    .phyStatePollFreqHz = 22000,
    .pollEnMask         = 0,
    .c45EnMask          = 0,
    .isMaster           = true,
    .disableStateMachineOnInit = true,
};

static const IcssgTimeSync_Cfg enetAppIcssgTimesyncCfg =
{
    .enable            = false,
    .clkType           = ICSSG_TIMESYNC_CLKTYPE_WORKING_CLOCK,
    .syncOut_start_WC  = 10000,
    .syncOut_pwidth_WC = 25000
};

typedef struct EnetAppInstInfo_LinkCfg
{
    Enet_Type enetType;
    uint32_t instId;
    EnetMacPort_LinkCfg linkCfgPort;
} EnetAppInstInfo_LinkCfg;

static const EnetAppInstInfo_LinkCfg enetAppMacPortLinkCfg[] =
{
    {
        .enetType = ENET_ICSSG_DUALMAC,
        .instId = 2,
        .linkCfgPort =
        {
            ENET_SPEED_AUTO,
            ENET_DUPLEX_AUTO,
        },
    },
    {
        .enetType = ENET_ICSSG_DUALMAC,
        .instId = 3,
        .linkCfgPort =
        {
            ENET_SPEED_AUTO,
            ENET_DUPLEX_AUTO,
        },
    },
};

static void EnetApp_initMdioConfig(Mdio_Cfg *pMdioCfg)
{
    *pMdioCfg = enetAppIcssgMdioCfg;
}

static void EnetApp_initTimesyncConfig(IcssgTimeSync_Cfg *pTimesyncCfg)
{
    *pTimesyncCfg = enetAppIcssgTimesyncCfg;
}

static void EnetApp_getIcssgInitCfg(Enet_Type enetType,
                                    uint32_t instId,
                                    Icssg_Cfg *pIcssgCfg)
{
    const uint32_t enetIndex = EnetApp_getEnetIdx(enetType, instId);
    EnetApp_initTimesyncConfig(&pIcssgCfg->timeSyncCfg);
    EnetApp_initMdioConfig(&pIcssgCfg->mdioCfg);
    EnetApp_macMode2MacMii(gInstInfo[enetIndex].rgmiiEn ? RGMII : MII, &pIcssgCfg->mii);
}

void EnetApp_getMacPortLinkCfg(Enet_Type enetType, uint32_t instId, EnetMacPort_LinkCfg *pMacPortLinkCfg, const Enet_MacPort portIdx)
{
    for (uint32_t i = 0; i <  ENET_ARRAYSIZE(enetAppMacPortLinkCfg); i++)
    {
        if ((enetAppMacPortLinkCfg[i].enetType == enetType) && (enetAppMacPortLinkCfg[i].instId == instId))
        {
            *pMacPortLinkCfg = enetAppMacPortLinkCfg[i].linkCfgPort;
            return;
        }
    }
    EnetAppUtils_assert(false);
}



