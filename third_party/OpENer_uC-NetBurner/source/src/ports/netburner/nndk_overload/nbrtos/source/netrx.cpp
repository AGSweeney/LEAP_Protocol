/*******************************************************************************
 * OpENer_uC-NetBurner — NetDoRX overload for libnetburner.a
 *
 * Calls CustomNetDoRX before ethertype dispatch so LLDP (0x88CC) reaches
 * opener_nb_lldp.cpp via SetCustomNetDoRX. Guarded indirect calls avoid crashes
 * from null or misaligned function pointers on Cortex-M (Thumb bit check).
 *
 * Link only via MKNBLIBS / NetBurner archive rebuild — do NOT add netrx.o to
 * USER_OBJS (duplicate NetDoRX symbol).
 ******************************************************************************/

#include <predef.h>

#include <buffers.h>
#include <constants.h>
#include <counters.h>
#include <ethernet.h>
#include <includes.h>
#include <ip.h>
#include <netinterface.h>
#include <netrx.h>
#include <nettimer.h>
#include <nettypes.h>
#include <snmp.h>
#include <stdint.h>

#ifdef IPV6
ProcessIp6Func *pIp6Func;
#endif

#ifdef ALLOW_CUSTOM_NET_DO_RX
netDoRXFunc CustomNetDoRX = NULL;
#endif

TimeOutManager NetTimeOutManager;
uint8_t PrimaryNetTimerTask;

extern void ProcessNBVLanID(PoolPtr p, InterfaceBlock *pifb);
void UnWrapVlan(PoolPtr pb, int len);
void FixVlanTx(PoolPtr pb, uint16_t vlanTag);

static inline bool netrx_fnptr_is_callable(uintptr_t fn)
{
    return fn != 0u && ((fn & 0x1u) != 0u);
}

int NetDoRX(PoolPtr pp, uint16_t ocount, int if_num)
{
    if (PrimaryNetTimerTask == 0)
    {
        PrimaryNetTimerTask = OSTaskID();
        NetTimeOutManager.InitTaskOwner();
    }

    if (pp)
    {
        frames_rx++;
        register pbeuint16_t cp;
        register uint32_t n;
        register uint32_t csum = 0;
        register uint16_t count = ocount;

#ifdef ALLOW_CUSTOM_NET_DO_RX
        int rv = 0;
#endif

        pp->bInterfaceNumber = if_num;

        cp = (pbeuint16_t)pp->pData;

        if (count < 14)
        {
            frames_rx_err++;
            FreeBuffer(pp);
            return NetTimeOutManager.ProcessEvents();
        }

#ifdef ALLOW_CUSTOM_NET_DO_RX
        if (CustomNetDoRX != NULL && netrx_fnptr_is_callable((uintptr_t)CustomNetDoRX))
        {
            rv = CustomNetDoRX(pp, ocount, if_num);
        }
        if (rv)
        {
            FreeBuffer(pp);
            return NetTimeOutManager.ProcessEvents();
        }
#endif

        count -= 14;
        n = 7;

        InterfaceBlock *ib = GetInterfaceBlock(if_num);

        if (cp[6] == ETHERNET_ETHERTYPE_VLAN)
        {
            bool bMatch = false;
            while ((ib) && (!bMatch))
            {
                if (ib->vlan_tag_value == (cp[7] & 0xFFF))
                    bMatch = true;
                else
                {
#if defined MULTIHOME
                    ib = ib->pChild;
#else
                    ib = NULL;
#endif
                }
            }

            if (bMatch)
            {
                UnWrapVlan(pp, ocount);
                ocount -= 4;
                count -= 4;
                pp->bBufferFlag |= BS_VLAN;
                if_num = GetInterfaceNumber(ib);
                pp->bInterfaceNumber = if_num;
            }
            else
            {
                if ((pp->bBufferFlag & BS_PHY_BCAST) && (cp[8 + 0] == ETHERNET_ETHERTYPE_IPv4) &&
                    (cp[8 + 12] == UDP_NETBURNERID_PORT) && (cp[8 + 15] == 0x4255) &&
                    (cp[8 + 16] == 0x564C) && ((cp[8 + 5] & 0x0FF) == IP_PROTOCOL_UDP))
                {
                    ProcessNBVLanID(pp, GetInterfaceBlock(if_num));
                    return NetTimeOutManager.ProcessEvents();
                }
                FreeBuffer(pp);
                return NetTimeOutManager.ProcessEvents();
            }
        }
        else if (ib != NULL && ib->vlan_tag_value)
        {
            if ((pp->bBufferFlag & BS_PHY_BCAST) && (cp[6 + 0] == ETHERNET_ETHERTYPE_IPv4) &&
                (cp[6 + 12] == UDP_NETBURNERID_PORT) && (cp[6 + 15] == 0x4255) &&
                (cp[6 + 16] == 0x564C) && ((cp[6 + 5] & 0x0FF) == IP_PROTOCOL_UDP))
            {
                ProcessNBVLanID(pp, GetInterfaceBlock(if_num));
                return NetTimeOutManager.ProcessEvents();
            }
            FreeBuffer(pp);
            return NetTimeOutManager.ProcessEvents();
        }

        if (cp[6] == ETHERNET_ETHERTYPE_IPv4)
        {
            pp->usedsize = 0;
            register uint16_t ip_count;

            if ((cp[7] & 0xFF00) != IP_20BYTE_ID)
            {
                register unsigned char option_count = (((cp[7] & 0x0F00) >> 8) - 5) << 2;
                if ((cp[7] & 0xF000) == (IP_20BYTE_ID & 0xF000))
                {
                    csum = (uint16_t)~GetSum((uint16_t *)cp + 7, 20 + option_count);
                    count -= option_count + 20;
                    ip_count = cp[8];
                    ip_count -= option_count + 20;
                    n += (20 + option_count) / 2;
                }
                else
                {
                    ip_count = 0;
                    csum = 0x1234;
                }
            }
            else
            {
                csum = (uint16_t)~GetSum20((puint32_t)(uint16_t *)(cp + 7));
                count -= 20;
                ip_count = cp[8];
                ip_count -= 20;
                n += 10;
            }

            if (csum == 0)
            {
                csum = 0;
                csum = GetSum((uint16_t *)cp + n, ip_count);
                pp->usedsize = (n * 2 + ip_count);
                count = 0;
            }

            if (pp->usedsize > 0)
            {
                ChangeOwner(pp);
                if (netrx_fnptr_is_callable((uintptr_t)pPacketfunc))
                {
                    pPacketfunc(pp, (PEFRAME)(cp), (csum));
                }
                else
                {
                    frames_rx_discard++;
                    FreeBuffer(pp);
                }
            }
            else
            {
                frames_rx_err++;
                enet_last_errhw |= 0xF000;
                SNMPCOUNT(snmp_var_ipInHdrErrors);
                FreeBuffer(pp);
            }
            return NetTimeOutManager.ProcessEvents();
        }

        if (cp[6] == ETHERNET_ETHERTYPE_ARP)
        {
            while (count)
            {
                n++;
                if (count == 1) { count = 0; }
                else { count -= 2; }
            }
            pp->usedsize = (n * 2);
            ChangeOwner(pp);
            if (netrx_fnptr_is_callable((uintptr_t)pArpFunc))
            {
                pArpFunc(pp, (PEFRAME)(cp));
            }
            else
            {
                frames_rx_discard++;
                FreeBuffer(pp);
            }
            return NetTimeOutManager.ProcessEvents();
        }

#ifdef IPV6
        if (cp[6] == ETHERNET_ETHERTYPE_IPv6)
        {
            if (netrx_fnptr_is_callable((uintptr_t)pIp6Func))
            {
                ChangeOwner(pp);
                pp->usedsize = ocount;
                pIp6Func(pp);
                return NetTimeOutManager.ProcessEvents();
            }
        }
#endif

        if (cp[6] < 0x600)
        {
            frames_rx_discard++;
            FreeBuffer(pp);
            return NetTimeOutManager.ProcessEvents();
        }

        frames_rx_discard++;
        FreeBuffer(pp);
        SNMPCOUNTIF(EthifInUnknownProtos, pp->bInterfaceNumber);
    }

    return NetTimeOutManager.ProcessEvents();
}

void UnWrapVlan(PoolPtr pb, int len)
{
    pbeuint32_t pdw = (pbeuint32_t)(pb->pData);
    int index = 3;
    while (index <= (len / 4))
    {
        pdw[index] = pdw[index + 1];
        index++;
    }
    pb->bBufferFlag = BS_VLAN;
}

void FixVlanTx(PoolPtr pb, uint16_t vlanTag)
{
    pbeuint32_t pdw = (pbeuint32_t)(&(pb->pData[0]));
    int index = pb->usedsize;
    index /= 4;
    index++;

    while (index >= 3)
    {
        pdw[index + 1] = pdw[index];
        index--;
    }

    PVLEFRAME pF = (PVLEFRAME)(pb->pData);
    pF->eVlType = ETHERNET_ETHERTYPE_VLAN;
    pF->eTag = vlanTag;

    if (pb->usedsize < ETH_MIN_SIZE) { pb->usedsize = ETH_MIN_SIZE + 4; }
    else
    {
        pb->usedsize += 4;
    }
}
