/* Firmware lifecycle: network prep → optional ACD/LLDP → CipStackInit → NetworkHandler. */

#include "opener.h"

#include "generic_networkhandler.h"
#include "networkconfig.h"
#include "cipidentity.h"
#include "opener_api.h"
#include "opener_nb_acd.h"
#if OPENER_NB_ACD
#include "opener_nb_acd_cip.h"
#endif
#include "opener_nb_config.h"
#include "opener_nb_identity.h"
#include "opener_nb_lldp.h"
#include "opener_nb_platform_types.h"
#include "opener_nb_ifconfig.h"
#include "trace.h"
#include "xorshiftrandom.h"
#include <stdio.h>

volatile int g_opener_abort = 0;
static int s_opener_running = 0;
static int s_opener_ifnum = OPENER_NB_DEFAULT_IFNUM;
extern void OpenerAppPrintAssemblyDetails(void);

static void OpenerPrintStartupBanner(void) {
  const opener_nb_ipv4_t ip = opener_nb_ipv4_from_u32_be(
    ntohl(g_tcpip.interface_configuration.ip_address));
  const opener_nb_ipv4_t mask = opener_nb_ipv4_from_u32_be(
    ntohl(g_tcpip.interface_configuration.network_mask));

  iprintf("\r\n");
  iprintf("  ___       _______ _   _            \r\n");
  iprintf(" / _ \\ _ __| ____| \\ | | ___ _ __   \r\n");
  iprintf("| | | | '_ \\  _| |  \\| |/ _ \\ '__|  \r\n");
  iprintf("| |_| | |_) | |___| |\\  |  __/ |     \r\n");
  iprintf(" \\___/| .__/|_____|_| \\_|\\___|_|     \r\n");
  iprintf("       |_|                              \r\n");
  iprintf("========================================\r\n");
  iprintf(" Welcome to OpENer on NetBurner\r\n");
  iprintf(" Network:\r\n");
  iprintf("  IP Address           : %u.%u.%u.%u\r\n",
          ip.octets[0], ip.octets[1], ip.octets[2], ip.octets[3]);
  iprintf("  Subnet Mask          : %u.%u.%u.%u\r\n",
          mask.octets[0], mask.octets[1], mask.octets[2], mask.octets[3]);
  iprintf(" Ports:\r\n");
  iprintf("  Explicit Messaging   : TCP/UDP %u\r\n",
          (unsigned int)kOpenerEthernetPort);
  iprintf("  I/O Messaging        : UDP %u\r\n",
          (unsigned int)kOpenerEipIoUdpPort);
  OpenerAppPrintAssemblyDetails();
  iprintf("========================================\r\n");
}

void opener_init(OpenerNetIfHandle netif) {
  EipStatus status = kEipStatusError;
  const int ifnum = OpenerNbNetifToIfnum(netif);
  s_opener_ifnum = ifnum;

  g_opener_abort = 0;
  s_opener_running = 0;

  status = OpenerNbPrepareNetworkStack(netif, 120);
  if(kEipStatusOk != status) {
    OPENER_TRACE_ERR("opener_init: network preparation failed\n");
    g_opener_abort = 1;
    return;
  }

#if OPENER_NB_ACD
  if(!OpenerNbAcdInit(ifnum)) {
    OPENER_TRACE_ERR("opener_init: ACD init failed (duplicate IP?)\n");
    g_opener_abort = 1;
    return;
  }
  OpenerNbAcdApplyTcpIpObject();
#endif

#if OPENER_NB_LLDP
  {
    OpenerNbLldpIdentity identity;
    OpenerNbLldpIdentityFromGlobals(&identity);
    if(!OpenerNbLldpInit(ifnum, &identity)) {
      OPENER_TRACE_ERR("opener_init: LLDP init failed\n");
      g_opener_abort = 1;
      return;
    }
  }
#endif

  SetXorShiftSeed(g_tcpip.interface_configuration.ip_address ^ 0xA5A5A5A5UL);
  status = CipStackInit((EipUint16)(NextXorShiftUint32() & 0xFFFFU));
  if(kEipStatusOk != status) {
    OPENER_TRACE_ERR("opener_init: CipStackInit failed\n");
    g_opener_abort = 1;
    return;
  }

  status = NetworkHandlerInitialize();
  if(kEipStatusOk != status) {
    OPENER_TRACE_ERR("opener_init: NetworkHandlerInitialize failed\n");
    ShutdownCipStack();
    g_opener_abort = 1;
    return;
  }

  CipIdentityEnterOperationalState();

  s_opener_running = 1;
  OpenerPrintStartupBanner();
  OPENER_TRACE_INFO("OpENer: initialized successfully\n");
}

void opener_process(void) {
  if((0 != g_opener_abort) || (0 == s_opener_running)) {
    return;
  }

  if(kEipStatusOk != NetworkHandlerProcessCyclic()) {
    OPENER_TRACE_ERR("opener_process: NetworkHandlerProcessCyclic failed\n");
    g_opener_abort = 1;
    return;
  }

#if OPENER_NB_ACD
  OpenerNbAcdPoll();
  OpenerNbAcdApplyTcpIpObject();
#endif
  OpenerNbSyncEthernetLinkFromNndk(s_opener_ifnum);
#if OPENER_NB_LLDP
  OpenerNbLldpPoll();
#endif
}

void opener_shutdown(void) {
  if(0 != s_opener_running) {
    s_opener_running = 0;
    g_opener_abort = 1;
    NetworkHandlerFinish();
#if OPENER_NB_LLDP
    OpenerNbLldpShutdown();
#endif
#if OPENER_NB_ACD
    OpenerNbAcdShutdown();
#endif
    ShutdownCipStack();
  }
}

int opener_get_status(void) {
  return g_opener_abort;
}
