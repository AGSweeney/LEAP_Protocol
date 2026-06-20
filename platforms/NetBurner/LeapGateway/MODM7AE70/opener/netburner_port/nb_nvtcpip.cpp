/*******************************************************************************
 * Persist CIP TCP/IP object settings to NetBurner InterfaceBlock flash config.
 ******************************************************************************/

#include "nb_nvtcpip.h"

#include <cstring>

#include <config_obj.h>
#include <netinterface.h>
#include <nettypes.h>

extern int g_opener_plant_ifnum;

#define kTcpipCfgCtrlStaticIp  0x00U
#define kTcpipCfgCtrlDhcp      0x02U
#define kTcpipCfgCtrlMethodMask 0x0FU

static uint32_t SwapIpv4Bytes(uint32_t value) {
  return ((value & 0x000000FFUL) << 24) |
         ((value & 0x0000FF00UL) << 8) |
         ((value & 0x00FF0000UL) >> 8) |
         ((value & 0xFF000000UL) >> 24);
}

static bool ModeIsStatic(const char *mode) {
  return (mode != NULL) &&
         ((strcmp(mode, "Static") == 0) || (strcmp(mode, "DHCP w Fallback") == 0));
}

static void ApplyModeToConfigControl(NbTcpIpNvConfig *cfg, const char *mode) {
  cfg->config_control &= ~kTcpipCfgCtrlMethodMask;
  if (strcmp(mode, "DHCP") == 0) {
    cfg->config_control |= kTcpipCfgCtrlDhcp;
  } else if (ModeIsStatic(mode)) {
    cfg->config_control |= kTcpipCfgCtrlStaticIp;
  } else {
    cfg->config_control |= kTcpipCfgCtrlDhcp;
  }
}

static void CopyStaticFieldsFromBlock(NbTcpIpNvConfig *cfg, const InterfaceBlock *block) {
  cfg->ip = SwapIpv4Bytes((uint32_t)static_cast<IPADDR4>(block->ip4.addr));
  cfg->mask = SwapIpv4Bytes((uint32_t)static_cast<IPADDR4>(block->ip4.mask));
  cfg->gateway = SwapIpv4Bytes((uint32_t)static_cast<IPADDR4>(block->ip4.gate));
  cfg->dns1 = SwapIpv4Bytes((uint32_t)static_cast<IPADDR4>(block->ip4.dns1));
  cfg->dns2 = SwapIpv4Bytes((uint32_t)static_cast<IPADDR4>(block->ip4.dns2));
}

extern "C" int nb_nv_tcpip_load(NbTcpIpNvConfig *cfg_out) {
  const int ifnum = (g_opener_plant_ifnum > 0) ? g_opener_plant_ifnum : GetFirstInterface();
  InterfaceBlock *block = GetInterfaceBlock(ifnum);

  if ((cfg_out == NULL) || (block == NULL)) {
    return -1;
  }

  memset(cfg_out, 0, sizeof(*cfg_out));
  const NBString modeStr = block->ip4.mode;
  const char *mode = modeStr.c_str();
  ApplyModeToConfigControl(cfg_out, mode);

  if (ModeIsStatic(mode)) {
    CopyStaticFieldsFromBlock(cfg_out, block);
  }

  return 0;
}

extern "C" int nb_nv_tcpip_store(const NbTcpIpNvConfig *cfg_in) {
  const int ifnum = (g_opener_plant_ifnum > 0) ? g_opener_plant_ifnum : GetFirstInterface();
  InterfaceBlock *block = GetInterfaceBlock(ifnum);

  if ((cfg_in == NULL) || (block == NULL)) {
    return -1;
  }

  const uint32_t method = cfg_in->config_control & kTcpipCfgCtrlMethodMask;
  if (method == kTcpipCfgCtrlDhcp) {
    block->ip4.mode = "DHCP";
  } else if (method == kTcpipCfgCtrlStaticIp) {
    block->ip4.mode = "Static";
    block->ip4.addr = IPADDR4(SwapIpv4Bytes(cfg_in->ip));
    block->ip4.mask = IPADDR4(SwapIpv4Bytes(cfg_in->mask));
    block->ip4.gate = IPADDR4(SwapIpv4Bytes(cfg_in->gateway));
    block->ip4.dns1 = IPADDR4(SwapIpv4Bytes(cfg_in->dns1));
    block->ip4.dns2 = IPADDR4(SwapIpv4Bytes(cfg_in->dns2));
  } else {
    return -1;
  }

  SaveConfigToStorage();
  return 0;
}
