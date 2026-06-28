#include "lldp.h"

LLDPEntity::LLDPEntity(InterfaceBlock &ib) : config_obj(ib, "LLDP", "LLDP Settings for this interface")
{
    m_pIfBlock = &ib;
    m_ActivePacket = 0;
    m_NewPacket = 0;
    m_bFastTx = true;
    m_SetTxTime = m_iTxTime;
    /* Do not RegisterInterval on NetTimeOutManager — TX is driven from poll task. */
}

void LLDPEntity::TimeElementEvent()
{
    /* Unused: this entity is not registered with the Enet task timer list. */
}

uint32_t LLDPEntity::ServiceTransmit()
{
    if (!m_bEnable) {
        return (uint32_t)m_iTxTime;
    }

    if (m_SetTxTime != m_iTxTime)
    {
        m_SetTxTime = m_iTxTime;
        m_bFastTx = true;
    }

    if (!m_pIfBlock->LinkActive())
    {
        return (uint32_t)m_iTxTime;
    }

    if ((m_ActivePacket == 0) && (m_NewPacket == 0))
    {
        BuildPacket();
    }

    OSCriticalSectionObj critical(m_Entity_Crit);
    if (m_ActivePacket != 0)
    {
        IncUsageCount(m_ActivePacket);
        m_pIfBlock->send_func(m_ActivePacket);
        if (m_bFastTx)
        {
            m_bFastTx = false;
            return (uint32_t)m_iRetransmitTime;
        }
    }

    return (uint32_t)m_iTxTime;
}

void LLDPEntity::StartNewPacket()
{
    if (m_NewPacket) FreeBuffer(m_NewPacket);
    m_NewPacket = GetBuffer();
    if (m_NewPacket == 0) return;
    PEFRAME pf = (PEFRAME)m_NewPacket->pData;
    m_NewPacket->usedsize = sizeof(EFRAME);
    MACADR ma_lldp;
    ma_lldp.phywadr[0] = 0x0180;
    ma_lldp.phywadr[1] = 0xC200;
    ma_lldp.phywadr[2] = 0x000E;

    pf->dest_addr = ma_lldp;
    pf->src_addr = m_pIfBlock->MAC;
    pf->eType = 0x88CC;
    AddMandatoryHeader();
}

void LLDPEntity::RawAdd(uint8_t id, unsigned int len, uint8_t *pData)
{
    if (m_NewPacket == 0) return;
    uint32_t siz = m_NewPacket->usedsize;
    puint8_t pPackData = m_NewPacket->pData + siz;

    if (len > 511) return;

    if (len > 256) { *pPackData++ = (id << 1) | 1; }
    else
    {
        *pPackData++ = (id << 1);
    }

    siz++;
    *pPackData++ = (uint8_t)(len & 0xff);
    siz++;

    for (uint32_t i = 0; i < len; i++)
    {
        *pPackData++ = pData[i];
        siz++;
    }
    m_NewPacket->usedsize = siz;
}

void LLDPEntity::RawAddString(uint8_t id, const char *str)
{
    RawAdd(id, strlen(str), (puint8_t)str);
}

void LLDPEntity::RawAddMac(uint8_t id, uint8_t subtype, MACADR &ma)
{
    uint8_t tb[7];
    tb[0] = subtype;
    for (int i = 0; i < 6; i++)
    {
        tb[i + 1] = ma.GetByte(i);
    }
    RawAdd(id, 7, tb);
}

void LLDPEntity::AddMandatoryHeader()
{
    MACADR ma;
    ma = InterfaceMAC(GetFirstInterface());
    RawAddMac(TLV_CHASSIS_ID, 4, ma);
    ma = m_pIfBlock->MAC;
    RawAddMac(TLV_PORT_ID, 3, ma);
    beuint16_t beto = (int)m_iHoldTime;
    RawAdd(TLV_TTL, 2, (puint8_t)&beto);
}

void LLDPEntity::AddSysCapabilities(uint16_t caps, uint16_t enabled)
{
    beuint32_t beu32;
    uint32_t val = caps;
    val = (val << 16) + enabled;
    beu32 = val;
    RawAdd(TLV_SYS_CAP, 4, (puint8_t)&beu32);
}

void LLDPEntity::AddManagmentAddr(IPADDR4 ipa)
{
    puint8_t pIPA = (puint8_t)&ipa;

    uint8_t buffer[12];

    buffer[0] = 5;
    buffer[1] = 1;
    buffer[2] = pIPA[0];
    buffer[3] = pIPA[1];
    buffer[4] = pIPA[2];
    buffer[5] = pIPA[3];
    buffer[6] = 0;
    buffer[7] = 0;
    buffer[8] = 0;
    buffer[9] = 0;
    buffer[10] = 0;
    buffer[11] = 0;
    RawAdd(TLV_MANAGMENT_ADDR, 12, buffer);
}

void LLDPEntity::AddCustomRaw(uint32_t UUID, uint32_t org_sub, uint32_t len, puint8_t pData)
{
    if (m_NewPacket == 0) return;

    uint32_t siz = m_NewPacket->usedsize;
    puint8_t pPackData = m_NewPacket->pData + siz;

    if (len > (511 - 4)) return;

    if (len > 252) { *pPackData++ = ((TLV_CUSTOM << 1) + 1); }
    else
    {
        *pPackData++ = TLV_CUSTOM << 1;
    }
    siz++;

    *pPackData++ = ((len + 4) & 0Xff);
    siz++;

    *pPackData++ = (UUID & 0xFF0000) >> 16;
    siz++;

    *pPackData++ = (UUID & 0xFF00) >> 8;
    siz++;

    *pPackData++ = UUID & (0xFF);
    siz++;

    *pPackData++ = org_sub;
    siz++;

    for (uint32_t i = 0; i < len; i++)
    {
        *pPackData++ = pData[i];
        siz++;
    }
    m_NewPacket->usedsize = siz;
}

void LLDPEntity::AddCustomInt(uint32_t UUID, uint32_t org_sub, int data, uint32_t intlen)
{
    beint32_t be = data;
    puint8_t pd = (puint8_t)&be;
    AddCustomRaw(UUID, org_sub, intlen, pd + (4 - intlen));
}

void LLDPEntity::AddCustomString(uint32_t UUID, uint32_t org_sub, const char *str)
{
    AddCustomRaw(UUID, org_sub, strlen(str), (puint8_t)str);
}

void LLDPEntity::UseNewPacket()
{
    RawAdd(0, 0, 0);
    {
        OSCriticalSectionObj critical(m_Entity_Crit);
        if (m_ActivePacket) FreeBuffer(m_ActivePacket);
        m_ActivePacket = m_NewPacket;
        m_NewPacket = 0;
    }
    m_bFastTx = true;
}

void LLDPEntity::BuildPacket()
{
    StartNewPacket();
    UseNewPacket();
}
