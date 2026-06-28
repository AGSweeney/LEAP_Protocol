/*******************************************************************************
 * LEAP Gateway OpENer application — 64-byte input/output assemblies 100/150.
 ******************************************************************************/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "opener_app_identity_conf.h"
#include "leap_gateway_eip_conf.h"
#include "leap_gateway_eip.h"
#include "cipidentity.h"
#include "cipqos.h"
#include "ciptcpipinterface.h"
#include "networkconfig.h"

static EipUint8 s_assembly_input[LEAP_GATEWAY_IO_ASSEMBLY_BYTES];
static EipUint8 s_assembly_output[LEAP_GATEWAY_IO_ASSEMBLY_BYTES];
static EipUint8 s_assembly_config[10];
static EipUint8 s_assembly_explicit[LEAP_GATEWAY_IO_ASSEMBLY_BYTES];

typedef char leap_gateway_assembly_size_check
  [(sizeof(s_assembly_input) == LEAP_GATEWAY_IO_ASSEMBLY_BYTES &&
    sizeof(s_assembly_output) == LEAP_GATEWAY_IO_ASSEMBLY_BYTES) ? 1 : -1];

static unsigned int s_io_connection_refcount = 0U;
static bool s_io_run_mode = false;

typedef enum OpenerResetAction {
  kOpenerResetNone = 0,
  kOpenerResetReboot = 1,
  kOpenerResetFactory = 2
} OpenerResetAction;

static volatile OpenerResetAction s_pending_reset_action = kOpenerResetNone;

extern void OpenerNbScheduleRebootNow(void);
extern void OpenerNbScheduleFactoryResetNow(void);

static void UpdateIdentityExtendedStatus(void)
{
  if(0U == s_io_connection_refcount) {
    CipIdentitySetExtendedDeviceStatus(kNoIoConnectionsEstablished);
  } else if(s_io_run_mode) {
    CipIdentitySetExtendedDeviceStatus(kAtLeastOneIoConnectionInRunMode);
  } else {
    CipIdentitySetExtendedDeviceStatus(
      kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
}

EipStatus ApplicationInitialization(void)
{
  SetDeviceVendorId((CipUint)OPENER_APP_IDENTITY_VENDOR_ID);
  SetDeviceType((EipUint16)OPENER_APP_IDENTITY_DEVICE_TYPE);
  SetDeviceProductCode((EipUint16)OPENER_APP_IDENTITY_PRODUCT_CODE);
  SetDeviceRevision((EipUint8)OPENER_APP_IDENTITY_MAJOR_REVISION,
                    (EipUint8)OPENER_APP_IDENTITY_MINOR_REVISION);
  SetDeviceProductName(OPENER_APP_IDENTITY_PRODUCT_NAME);

  CreateAssemblyObject(LEAP_GATEWAY_INPUT_ASSEMBLY_NUM, s_assembly_input,
                       LEAP_GATEWAY_IO_ASSEMBLY_BYTES);
  CreateAssemblyObject(LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM, s_assembly_output,
                       LEAP_GATEWAY_IO_ASSEMBLY_BYTES);
  CreateAssemblyObject(LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM, s_assembly_config,
                       (EipUint16)sizeof(s_assembly_config));
  CreateAssemblyObject(LEAP_GATEWAY_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(LEAP_GATEWAY_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(LEAP_GATEWAY_EXPLICIT_ASSEMBLY_NUM, s_assembly_explicit,
                       LEAP_GATEWAY_IO_ASSEMBLY_BYTES);

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

  OpenerConfigureMicro800RunIdleCompat();

  return kEipStatusOk;
}

void HandleApplication(void)
{
  const OpenerResetAction pending_action = s_pending_reset_action;
  if(kOpenerResetNone == pending_action) {
    return;
  }

  s_pending_reset_action = kOpenerResetNone;

  if(kOpenerResetFactory == pending_action) {
    OPENER_TRACE_INFO("LEAP Gateway: factory reset requested\r\n");
    OpenerNbScheduleFactoryResetNow();
  } else {
    OPENER_TRACE_INFO("LEAP Gateway: reboot requested via Identity Reset service\r\n");
    OpenerNbScheduleRebootNow();
  }
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event)
{
  (void)output_assembly_id;
  (void)input_assembly_id;

  if(kIoConnectionEventOpened == io_connection_event) {
    ++s_io_connection_refcount;
    UpdateIdentityExtendedStatus();
  } else if((kIoConnectionEventClosed == io_connection_event) ||
            (kIoConnectionEventTimedOut == io_connection_event)) {
    if(s_io_connection_refcount > 0U) {
      --s_io_connection_refcount;
    }
    if(0U == s_io_connection_refcount) {
      s_io_run_mode = false;
    }
    UpdateIdentityExtendedStatus();
  }
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance)
{
  switch(instance->instance_number) {
    case LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM:
      leap_gateway_eip_apply_output_assembly(
        s_assembly_output,
        LEAP_GATEWAY_IO_ASSEMBLY_BYTES);
      break;
    default:
      break;
  }
  return kEipStatusOk;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *instance)
{
  size_t packed_len = 0U;

  if(instance->instance_number == LEAP_GATEWAY_INPUT_ASSEMBLY_NUM) {
    leap_gateway_eip_pack_input_assembly(
      s_assembly_input,
      LEAP_GATEWAY_IO_ASSEMBLY_BYTES,
      &packed_len);
  }

  return true;
}

EipStatus ResetDevice(void)
{
  CloseAllConnections();
  if(kEipStatusOk != OpenerNbApplyTcpIpConfiguration(NULL)) {
    OPENER_TRACE_WARN("LEAP Gateway: reset type 0 continuing after TCP/IP apply failure\r\n");
  }
  CipQosUpdateUsedSetQosValues();
  s_pending_reset_action = kOpenerResetReboot;
  return kEipStatusOk;
}

EipStatus ResetDeviceToInitialConfiguration(void)
{
  CloseAllConnections();
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  s_pending_reset_action = kOpenerResetFactory;
  return kEipStatusOk;
}

void RunIdleChanged(EipUint32 run_idle_value)
{
  s_io_run_mode = ((run_idle_value & 0x0001U) != 0U);
  UpdateIdentityExtendedStatus();
}

void OpenerAppPrintAssemblyDetails(void)
{
  iprintf(" Assemblies:\r\n");
  iprintf("  Input (T->O)    : %u (%u bytes)\r\n",
          LEAP_GATEWAY_INPUT_ASSEMBLY_NUM,
          (unsigned int)LEAP_GATEWAY_IO_ASSEMBLY_BYTES);
  iprintf("  Output (O->T)   : %u (%u bytes)\r\n",
          LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM,
          (unsigned int)LEAP_GATEWAY_IO_ASSEMBLY_BYTES);
  iprintf("  Config          : %u (%u bytes)\r\n",
          LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM,
          (unsigned int)sizeof(s_assembly_config));
  iprintf("  Heartbeat In    : %u (0 bytes)\r\n",
          LEAP_GATEWAY_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM);
  iprintf("  Heartbeat Listen: %u (0 bytes)\r\n",
          LEAP_GATEWAY_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM);
  iprintf("  Explicit Msg    : %u (%u bytes)\r\n",
          LEAP_GATEWAY_EXPLICIT_ASSEMBLY_NUM,
          (unsigned int)LEAP_GATEWAY_IO_ASSEMBLY_BYTES);
}
