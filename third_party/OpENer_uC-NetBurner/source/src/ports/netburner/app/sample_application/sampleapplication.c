/* Demo assemblies and CIP callbacks — replace with your product I/O map and EDS. */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "opener_app_identity_conf.h"
#include "cipidentity.h"
#include "cipqos.h"
#include "ciptcpipinterface.h"
#include "networkconfig.h"
#include "opener_nb_acd.h"

/** Input assembly (T->O to scanner) — demo 32 bytes. */
#define DEMO_APP_INPUT_ASSEMBLY_NUM 100
/** Output assembly (O->T from scanner) — demo 32 bytes. */
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM 150
#define DEMO_APP_CONFIG_ASSEMBLY_NUM 151
#define DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM 152
#define DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM 153
#define DEMO_APP_EXPLICT_ASSEMBLY_NUM 154

static EipUint8 s_assembly_data064[32];
static EipUint8 s_assembly_data096[32];
static EipUint8 s_assembly_data097[10];
static EipUint8 s_assembly_data09A[32];
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

static void UpdateIdentityExtendedStatus(void) {
  if(0U == s_io_connection_refcount) {
    CipIdentitySetExtendedDeviceStatus(kNoIoConnectionsEstablished);
  } else if(s_io_run_mode) {
    CipIdentitySetExtendedDeviceStatus(kAtLeastOneIoConnectionInRunMode);
  } else {
    CipIdentitySetExtendedDeviceStatus(
      kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
}

EipStatus ApplicationInitialization(void) {
  /* Project-local identity surface (opener_user_conf.h) */
  SetDeviceVendorId((CipUint)OPENER_APP_IDENTITY_VENDOR_ID);
  SetDeviceType((EipUint16)OPENER_APP_IDENTITY_DEVICE_TYPE);
  SetDeviceProductCode((EipUint16)OPENER_APP_IDENTITY_PRODUCT_CODE);
  SetDeviceRevision((EipUint8)OPENER_APP_IDENTITY_MAJOR_REVISION,
                    (EipUint8)OPENER_APP_IDENTITY_MINOR_REVISION);
  SetDeviceProductName(OPENER_APP_IDENTITY_PRODUCT_NAME);

  CreateAssemblyObject(DEMO_APP_INPUT_ASSEMBLY_NUM, s_assembly_data064,
                       sizeof(s_assembly_data064));
  CreateAssemblyObject(DEMO_APP_OUTPUT_ASSEMBLY_NUM, s_assembly_data096,
                       sizeof(s_assembly_data096));
  CreateAssemblyObject(DEMO_APP_CONFIG_ASSEMBLY_NUM, s_assembly_data097,
                       sizeof(s_assembly_data097));
  CreateAssemblyObject(DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(DEMO_APP_EXPLICT_ASSEMBLY_NUM, s_assembly_data09A,
                       sizeof(s_assembly_data09A));

  ConfigureExclusiveOwnerConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
                                       DEMO_APP_INPUT_ASSEMBLY_NUM,
                                       DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureInputOnlyConnectionPoint(0, DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM,
                                    DEMO_APP_INPUT_ASSEMBLY_NUM,
                                    DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureListenOnlyConnectionPoint(0, DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM,
                                     DEMO_APP_INPUT_ASSEMBLY_NUM,
                                     DEMO_APP_CONFIG_ASSEMBLY_NUM);

  OpenerConfigureMicro800RunIdleCompat();

  return kEipStatusOk;
}

/**
 * @brief Optional periodic application work — called from stack when idle.
 *
 * Keep this fast; real-time I/O belongs in AfterAssemblyDataReceived / BeforeAssemblyDataSend.
 */
void HandleApplication(void) {
  const OpenerResetAction pending_action = s_pending_reset_action;
  if(kOpenerResetNone == pending_action) {
    return;
  }

  s_pending_reset_action = kOpenerResetNone;

  if(kOpenerResetFactory == pending_action) {
    OPENER_TRACE_INFO("OpENer: factory reset requested\n");
    OpenerNbScheduleFactoryResetNow();
  } else {
    OPENER_TRACE_INFO("OpENer: reboot requested via Identity Reset service\n");
    OpenerNbScheduleRebootNow();
  }
}

/**
 * @brief Forward Open / Close / timeout notifications for I/O connections.
 *
 * Demo updates Identity extended status for CT. When OPENER_NB_ACD is enabled,
 * notifies ACD semi-active probe logic via OpenerNbAcdNotifyIoConnection().
 */
void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event) {
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

#if defined(OPENER_NB_ACD) && OPENER_NB_ACD
  if(kIoConnectionEventOpened == io_connection_event) {
    OpenerNbAcdNotifyIoConnection(true);
  } else if(kIoConnectionEventClosed == io_connection_event ||
            kIoConnectionEventTimedOut == io_connection_event) {
    OpenerNbAcdNotifyIoConnection(false);
  }
#endif
}

/**
 * @brief O->T assembly data arrived — copy to application buffers / hardware outputs.
 *
 * Demo mirrors output assembly 150 into input assembly 100 for loopback testing.
 */
EipStatus AfterAssemblyDataReceived(CipInstance *instance) {
  switch(instance->instance_number) {
    case DEMO_APP_OUTPUT_ASSEMBLY_NUM:
      memcpy(s_assembly_data064, s_assembly_data096, sizeof(s_assembly_data064));
      break;
    default:
      break;
  }
  return kEipStatusOk;
}

/**
 * @brief T->O assembly about to transmit — refresh input data before send.
 *
 * Return false to suppress this transmission cycle (rare).
 */
EipBool8 BeforeAssemblyDataSend(CipInstance *instance) {
  (void)instance;
  return true;
}

/** @brief CIP Reset service — close connections and reapply QoS. */
EipStatus ResetDevice(void) {
  CloseAllConnections();
  if(kEipStatusOk != OpenerNbApplyTcpIpConfiguration(NULL)) {
    OPENER_TRACE_WARN("OpENer: reset type 0 continuing after TCP/IP apply failure\n");
  }
  CipQosUpdateUsedSetQosValues();
  s_pending_reset_action = kOpenerResetReboot;
  return kEipStatusOk;
}

/** @brief Reset to default attribute values then ResetDevice(). */
EipStatus ResetDeviceToInitialConfiguration(void) {
  CloseAllConnections();
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  s_pending_reset_action = kOpenerResetFactory;
  return kEipStatusOk;
}

/**
 * @brief Run/Idle header parsed on O->T connection (when declared header mode is on).
 *
 * Not used when OpenerConfigureMicro800RunIdleCompat() strips undeclared headers.
 */
void RunIdleChanged(EipUint32 run_idle_value) {
  s_io_run_mode = ((run_idle_value & 0x0001U) != 0U);
  UpdateIdentityExtendedStatus();
}

/** @brief Print demo assembly map used by this sample application. */
void OpenerAppPrintAssemblyDetails(void) {
  iprintf(" Assemblies:\r\n");
  iprintf("  Input (T->O)    : %u (%u bytes)\r\n",
          DEMO_APP_INPUT_ASSEMBLY_NUM, (unsigned int)sizeof(s_assembly_data064));
  iprintf("  Output (O->T)   : %u (%u bytes)\r\n",
          DEMO_APP_OUTPUT_ASSEMBLY_NUM, (unsigned int)sizeof(s_assembly_data096));
  iprintf("  Config          : %u (%u bytes)\r\n",
          DEMO_APP_CONFIG_ASSEMBLY_NUM, (unsigned int)sizeof(s_assembly_data097));
  iprintf("  Heartbeat In    : %u (0 bytes)\r\n",
          DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM);
  iprintf("  Heartbeat Listen: %u (0 bytes)\r\n",
          DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM);
  iprintf("  Explicit Msg    : %u (%u bytes)\r\n",
          DEMO_APP_EXPLICT_ASSEMBLY_NUM, (unsigned int)sizeof(s_assembly_data09A));
}
