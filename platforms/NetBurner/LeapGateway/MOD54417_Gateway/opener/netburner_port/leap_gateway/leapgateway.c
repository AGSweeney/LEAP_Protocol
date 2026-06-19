/*******************************************************************************
 * LeapGateway - OpENer EtherNet/IP application for LEAP Gateway on NetBurner.
 *
 * Assembly layout and connection points follow OpENer CT17 Communications Adapter
 * reference (assemblies 100/150/151 + heartbeat 152/153 + explicit 154).
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "nvdata.h"
#include "cipcommon.h"
#include "ethlinkcbs.h"
#include "nvtcpip.h"
#include "nvqos.h"
#include "cipstring.h"
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
#include "cipethernetlink.h"
#endif

extern void nb_schedule_reboot(void);

#define LEAP_GATEWAY_INPUT_ASSEMBLY_NUM  100U
#define LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM 150U
#define LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM 151U
#define LEAP_GATEWAY_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM  152U
#define LEAP_GATEWAY_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM 153U
#define LEAP_GATEWAY_EXPLICIT_ASSEMBLY_NUM 154U

EipUint8 g_assembly_data064[32];
EipUint8 g_assembly_data096[32];
EipUint8 g_assembly_data097[10];
EipUint8 g_assembly_data09A[32];

static EipUint32 s_active_io_connections = 0U;
static bool s_io_activity_seen = false;

__attribute__((weak)) void
leap_gateway_eip_apply_output_assembly(const EipUint8 *data, size_t length)
{
  (void)data;
  (void)length;
}

__attribute__((weak)) void
leap_gateway_eip_pack_input_assembly(EipUint8 *data, size_t capacity, size_t *length)
{
  if ((data != NULL) && (capacity > 0U) && (length != NULL)) {
    memset(data, 0, capacity);
    *length = capacity;
  }
}

static void IdentityEnter(CipIdentityState state,
                          CipIdentityExtendedStatus ext_status)
{
  if (g_identity.state != (CipUsint)state) {
    OPENER_TRACE_INFO("Identity state -> %u\n", (unsigned)state);
    g_identity.state = (CipUsint)state;
  }
  CipIdentitySetExtendedDeviceStatus(ext_status);
}

static void IdentityFlagFault(bool fatal)
{
  CipWord flag = fatal ? kMajorUnrecoverableFault : kMajorRecoverableFault;

  CipIdentitySetStatusFlags(flag);
  IdentityEnter(fatal ? kStateMajorUnrecoverableFault
                      : kStateMajorRecoverableFault,
                kMajorFault);
}

static void IdentityNoteIoActivity(void)
{
  if (s_active_io_connections > 0U) {
    s_io_activity_seen = true;
    IdentityEnter(kStateOperational, kAtLeastOneIoConnectionInRunMode);
  }
}

static void RegisterEthernetLinkCallbacks(void)
{
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  CipClass *eth_link_class = GetCipClass(kCipEthernetLinkClassCode);
  int idx;

  InsertGetSetCallback(eth_link_class, EthLnkPreGetCallback, kPreGetFunc);
  InsertGetSetCallback(eth_link_class, EthLnkPostGetCallback, kPostGetFunc);

  for (idx = 0; idx < OPENER_ETHLINK_INSTANCE_CNT; ++idx) {
    CipInstance *eth_link_inst =
      GetCipInstance(eth_link_class, (CipInstanceNum)(idx + 1));
    CipAttributeStruct *eth_link_attr;

    OPENER_ASSERT(eth_link_inst != NULL);

    eth_link_attr = GetCipAttribute(eth_link_inst, 4);
    eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);

    eth_link_attr = GetCipAttribute(eth_link_inst, 5);
    eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
  }
#endif

  EthLnkRegisterInterfaceState();
}

EipStatus ApplicationInitialization(void)
{
  CreateAssemblyObject(LEAP_GATEWAY_INPUT_ASSEMBLY_NUM, g_assembly_data064,
                       sizeof(g_assembly_data064));
  CreateAssemblyObject(LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM, g_assembly_data096,
                       sizeof(g_assembly_data096));
  CreateAssemblyObject(LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM, g_assembly_data097,
                       sizeof(g_assembly_data097));
  CreateAssemblyObject(LEAP_GATEWAY_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(LEAP_GATEWAY_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(LEAP_GATEWAY_EXPLICIT_ASSEMBLY_NUM, g_assembly_data09A,
                       sizeof(g_assembly_data09A));

  ConfigureExclusiveOwnerConnectionPoint(
    0,
    LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM,
    LEAP_GATEWAY_INPUT_ASSEMBLY_NUM,
    LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM);
  ConfigureInputOnlyConnectionPoint(
    0,
    LEAP_GATEWAY_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM,
    LEAP_GATEWAY_INPUT_ASSEMBLY_NUM,
    LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM);
  ConfigureListenOnlyConnectionPoint(
    0,
    LEAP_GATEWAY_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM,
    LEAP_GATEWAY_INPUT_ASSEMBLY_NUM,
    LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM);

  CipRunIdleHeaderSetO2T(false);
  CipRunIdleHeaderSetT2O(false);

  InsertGetSetCallback(GetCipClass(kCipQoSClassCode), NvQosSetCallback, kNvDataFunc);
  InsertGetSetCallback(GetCipClass(kCipTcpIpInterfaceClassCode), NvTcpipSetCallback,
                       kNvDataFunc);

  RegisterEthernetLinkCallbacks();

  s_active_io_connections = 0U;
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
  s_io_activity_seen = false;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);

  return kEipStatusOk;
}

void HandleApplication(void)
{
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event)
{
  (void)output_assembly_id;
  (void)input_assembly_id;

  switch (io_connection_event) {
    case kIoConnectionEventOpened:
      if (s_active_io_connections++ == 0U) {
        IdentityEnter(kStateStandby,
                      kAtLeastOneIoConnectionEstablishedAllInIdleMode);
      }
      break;
    case kIoConnectionEventTimedOut:
    case kIoConnectionEventClosed:
      if (s_active_io_connections > 0U) {
        s_active_io_connections--;
      }
      if (s_active_io_connections == 0U) {
        s_io_activity_seen = false;
        IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
      }
      break;
    default:
      break;
  }
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance)
{
  switch (instance->instance_number) {
    case LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM:
      IdentityNoteIoActivity();
      leap_gateway_eip_apply_output_assembly(
        g_assembly_data096,
        sizeof(g_assembly_data096));
      return kEipStatusOk;
    case LEAP_GATEWAY_EXPLICIT_ASSEMBLY_NUM:
      return kEipStatusOk;
    case LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM:
      return kEipStatusOk;
    default:
      return kEipStatusOk;
  }
}

EipBool8 BeforeAssemblyDataSend(CipInstance *instance)
{
  size_t packed_len = 0U;

  if (instance->instance_number == LEAP_GATEWAY_INPUT_ASSEMBLY_NUM) {
    leap_gateway_eip_pack_input_assembly(
      g_assembly_data064,
      sizeof(g_assembly_data064),
      &packed_len);
    IdentityNoteIoActivity();
  }

  return true;
}

static void RestoreTcpIpDefaults(void)
{
  g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
  g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
  g_tcpip.interface_configuration.ip_address = 0U;
  g_tcpip.interface_configuration.network_mask = 0U;
  g_tcpip.interface_configuration.gateway = 0U;
  g_tcpip.interface_configuration.name_server = 0U;
  g_tcpip.interface_configuration.name_server_2 = 0U;
  ClearCipString(&g_tcpip.interface_configuration.domain_name);
  ClearCipString(&g_tcpip.hostname);
  g_tcpip.status |= kTcpipStatusIfaceCfgPend;
  (void)NvTcpipStore(&g_tcpip);
}

EipStatus ResetDevice(void)
{
  nb_schedule_reboot();
  return kEipStatusOk;
}

void *CipCalloc(size_t number_of_elements, size_t size_of_element)
{
  return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data)
{
  free(data);
}

EipStatus ResetDeviceToInitialConfiguration(void)
{
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  (void)NvQosStore(&g_qos);
  RestoreTcpIpDefaults();
  nb_schedule_reboot();
  return kEipStatusOk;
}

void RunIdleChanged(EipUint32 run_idle_value)
{
  if ((run_idle_value & 0x0001U) != 0U) {
    IdentityNoteIoActivity();
  } else if (s_active_io_connections == 0U) {
    IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
  } else if (!s_io_activity_seen) {
    IdentityEnter(kStateStandby,
                  kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
}

void ApplicationNotifyLinkUp(void)
{
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateEnabled);
  IdentityEnter(kStateStandby,
                s_active_io_connections > 0U
                  ? kAtLeastOneIoConnectionEstablishedAllInIdleMode
                  : kNoIoConnectionsEstablished);
}

void ApplicationNotifyLinkDown(void)
{
  s_active_io_connections = 0U;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);
  s_io_activity_seen = false;
  IdentityFlagFault(false);
}
