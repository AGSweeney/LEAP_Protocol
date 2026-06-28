#ifndef _LLDP_H
#define _LLDP_H 1

#include <netinterface.h>
#include <nettypes.h>
#include <config_obj.h>

#define TLV_CHASSIS_ID 1
#define TLV_PORT_ID 2
#define TLV_TTL 3
#define TLV_PORT_DESC 4
#define TLV_SYS_NAME 5
#define TLV_SYS_DESC 6
#define TLV_SYS_CAP 7
#define TLV_MANAGMENT_ADDR 8
#define TLV_CUSTOM 127

class LLDPEntity : public TimeOutElement, public config_obj
{
   private:
    int m_SetTxTime;
    PoolPtr m_ActivePacket;
    PoolPtr m_NewPacket;
    InterfaceBlock *m_pIfBlock;
    OS_CRIT m_Entity_Crit;
    bool m_bFastTx;

    virtual void TimeElementEvent();
    void RawAdd(uint8_t id, unsigned int len, uint8_t *pData);
    void RawAddString(uint8_t id, const char *str);
    void RawAddMac(uint8_t id, uint8_t sub_type, MACADR &ma);
    void AddMandatoryHeader();
   protected:
    void StartNewPacket();
    void AddPortDescription(const char *Description) { RawAddString(TLV_PORT_DESC, Description); };
    void AddHostName(const char *HostName) { RawAddString(TLV_SYS_NAME, HostName); };
    void AddSysDescription(const char *Description) { RawAddString(TLV_SYS_DESC, Description); };
    void AddSysCapabilities(uint16_t capablities, uint16_t enabled);
    void AddManagmentAddr(IPADDR4 ipa);
    void AddCustomRaw(uint32_t UUID, uint32_t org_sub, uint32_t datalen, puint8_t data);
    void AddCustomInt(uint32_t UUID, uint32_t org_sub, int data, uint32_t intlen);
    void AddCustomString(uint32_t UUID, uint32_t org_sub, const char *str);
    void UseNewPacket();

   public:
    config_bool m_bEnable{TRUE, "Enable", "Enable/disable LLDP transmisions"};
    config_int m_iTxTime{30, "TxTimer", "Transmission interval in seconds"};
    config_int m_iHoldTime{120, "HoldTime", "LLD TTL"};
    config_int m_iRetransmitTime{2, "RetransmitTime", "Seconds to wait on Startup"};
    ConfigEndMarker;

    LLDPEntity(InterfaceBlock &ib);
    virtual void BuildPacket();
    void RebuildPacket() { BuildPacket(); };

    /** Run LLDP TX from a normal task (not Enet#26). Returns seconds until next send. */
    uint32_t ServiceTransmit();
    void RequestFastTransmit() { m_bFastTx = true; }
};

#endif
