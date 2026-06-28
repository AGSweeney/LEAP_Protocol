/* Revision: 3.5.7 */

/******************************************************************************
* Copyright 1998-2024 NetBurner, Inc.  ALL RIGHTS RESERVED
*
*    Permission is hereby granted to purchasers of NetBurner Hardware to use or
*    modify this computer program for any use as long as the resultant program
*    is only executed on NetBurner provided hardware.
*
*    No other rights to use this program or its derivatives in part or in
*    whole are granted.
*
*    It may be possible to license this or other NetBurner software for use on
*    non-NetBurner Hardware. Contact sales@Netburner.com for more information.
*
*    NetBurner makes no representation or warranties with respect to the
*    performance of this computer program, and specifically disclaims any
*    responsibility for any damages, special or consequential, connected with
*    the use of this program.
*
* NetBurner
* 16855 W Bernardo Dr
* San Diego, CA 92127
* www.netburner.com
******************************************************************************/

/*
 * LEAP port note:
 * Force-enable custom NetDoRX hook support in the NetBurner archive build
 * so SetCustomNetDoRX() receives raw EtherType frames.
 */
#define ALLOW_CUSTOM_NET_DO_RX

// NB Definitions
#include <predef.h>

// NB Libs
#include <hal.h>
#include <buffers.h>
#include <constants.h>
#include <counters.h>
#include <ethernet.h>
#include <includes.h>
#include <ip.h>
#include <netinterface.h>
#include <netrx.h>
#include <nettypes.h>
#include <randseed.h>
#include <sim.h>
#include <snmp.h>
#include <stdio.h>
#include <system.h>
#include <utils.h>

#ifdef IPV6
ProcessIp6Func *pIp6Func;
#endif

#ifdef ALLOW_CUSTOM_NET_DO_RX
netDoRXFunc CustomNetDoRX = NULL;
#endif

TimeOutManager NetTimeOutManager;
uint8_t PrimaryNetTimerTask;

void UnWrapVlan(PoolPtr pp, int len);
void ProcessNBVLanID(PoolPtr p, InterfaceBlock *pifb);

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
        // check if any custom ethernet handler has been registered
        if (CustomNetDoRX) { rv = CustomNetDoRX(pp, ocount, if_num); }
        // if the handler processed the frame, then free it and return
        if (rv)
        {
            // iprintf("NETRX Bailing Custom Do Rx");
            FreeBuffer(pp);
            // iprintf("Process events 2\r\n");
            return NetTimeOutManager.ProcessEvents();
        }
#endif
        count -= 14;
        n = 7;

        InterfaceBlock *ib = GetInterfaceBlock(if_num);

        if (cp[6] == ETHERNET_ETHERTYPE_VLAN)   // Vlan potentially unwrap!
        {
            // cp[7] holds the potential vlan....
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
                //        printf("RXVL on %d\n",if_num);
            }
            else
            {   // Had Vlan and we are not vlan. Look for NB search packet

                if ((pp->bBufferFlag & BS_PHY_BCAST) && (cp[8 + 0] == ETHERNET_ETHERTYPE_IPv4) && (cp[8 + 12] == UDP_NETBURNERID_PORT) &&
                    (cp[8 + 15] == 0x4255) && (cp[8 + 16] == 0x564C) && ((cp[8 + 5] & 0x0FF) == IP_PROTOCOL_UDP))
                {
                    ProcessNBVLanID(pp, GetInterfaceBlock(if_num));
                    return NetTimeOutManager.ProcessEvents();
                }
                else
                {
                    FreeBuffer(pp);
                    return NetTimeOutManager.ProcessEvents();
                    ;
                }
            }
        }   // Had vlan tag
        else
        {   // No vlan
            if (ib->vlan_tag_value)
            {   // Root vlan and no vlan sent
                if ((pp->bBufferFlag & BS_PHY_BCAST) && (cp[6 + 0] == ETHERNET_ETHERTYPE_IPv4) && (cp[6 + 12] == UDP_NETBURNERID_PORT) &&
                    (cp[6 + 15] == 0x4255) && (cp[6 + 16] == 0x564C) && ((cp[6 + 5] & 0x0FF) == IP_PROTOCOL_UDP))
                {
                    ProcessNBVLanID(pp, GetInterfaceBlock(if_num));
                    return NetTimeOutManager.ProcessEvents();
                }
                else
                {
                    FreeBuffer(pp);
                    return NetTimeOutManager.ProcessEvents();
                }
            }
        }   // Was not vlan

        if (cp[6] == ETHERNET_ETHERTYPE_IPv4)   // Type is IP
        {
            pp->usedsize = 0;
            // Process IP packet
            // Now we read the IP header.
            // We need to do the checksum stuff
            DBPRINT(DB_ETHER, "I");

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
                    csum = 0x1234; /* A Bogus value */
                }
            }
            else
            {
                csum = (uint16_t)~GetSum20((puint32_t)(uint16_t *)(cp + 7));
                // csum = GetSum( cp + 7, 20 );
                count -= 20;
                ip_count = cp[8];
                ip_count -= 20;
                n += 10;
            }

            // Now we should have a 32 bit checksum value for the header.
            // See RFC1071 for information on calculating the checksum.
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
                // iprintf("Forward packet\r\n");
                pPacketfunc(pp, (PEFRAME)(cp), (csum));
            }
            else
            {
                frames_rx_err++;
                enet_last_errhw |= 0xF000;
                SNMPCOUNT(snmp_var_ipInHdrErrors);   // inc for IP header checksum error (by R.A)
                // iprintf("NETRX Bailing header csum");
                FreeBuffer(pp);
            }
            // iprintf("Process events3\r\n");
            return NetTimeOutManager.ProcessEvents();

        } /* Was IP packet */

        if (cp[6] == ETHERNET_ETHERTYPE_ARP)
        {
            DBPRINT(DB_ETHER, "A");
            while (count)
            {
                n++;
                if (count == 1) { count = 0; }
                else
                {
                    count -= 2;
                }
            }
            pp->usedsize = (n * 2);

            // Fixup the next packet pointer and recieve buffer boundary
            ChangeOwner(pp);
            pArpFunc(pp, (PEFRAME)(cp));
            //	  iprintf("Process events 4\r\n");
            return NetTimeOutManager.ProcessEvents();
        }

#ifdef IPV6
        if (cp[6] == ETHERNET_ETHERTYPE_IPv6)
        {
            if (pIp6Func)
            {
                ChangeOwner(pp);
                pp->usedsize = ocount;
                pIp6Func(pp);
                // iprintf("Process events5\r\n");

                return NetTimeOutManager.ProcessEvents();
            }
        }
#endif

        if (cp[6] < 0x600)
        {   // 802.3 frames
            frames_rx_discard++;
            FreeBuffer(pp);
            // iprintf("Process events6\r\n");

            return NetTimeOutManager.ProcessEvents();
        }
        // iprintf("NETRX Bailing No Handler for %04X",cp[6]);
        // iprintf("Unhandled frame to mac:");
        // ShowMac((MACADR * )cp);
        // iprintf("\r\n");

        frames_rx_discard++;
        FreeBuffer(pp);
        SNMPCOUNT(snmp_var_ipInUnknownProtos);   // Rx valid packet but unknown protocol (R.A)
    }
    // pp was null
    // iprintf("Process events 7\r\n");
    return NetTimeOutManager.ProcessEvents();
}

extern "C" int leap_net_process_events(void)
{
    return NetTimeOutManager.ProcessEvents();
}

void UnWrapVlan(PoolPtr pb, int len)
{
    // Move the world
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

    // We are moving part of destination over stuff that will be rewritten later
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
