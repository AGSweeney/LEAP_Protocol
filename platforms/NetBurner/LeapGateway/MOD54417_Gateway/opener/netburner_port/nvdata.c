/*******************************************************************************
 * Non-volatile data load/store callbacks for LEAP Gateway on NetBurner.
 ******************************************************************************/

#include "nvdata.h"

#include "trace.h"
#include "cipqos.h"
#include "nvqos.h"
#include "nvtcpip.h"

EipStatus NvdataLoad(void) {
  EipStatus eip_status = kEipStatusOk;

  if (kEipStatusOk != NvTcpipLoad(&g_tcpip)) {
    OPENER_TRACE_INFO("NvdataLoad: TCP/IP NV not loaded, using stack defaults\n");
  }

  if (kEipStatusOk != NvQosLoad(&g_qos)) {
    eip_status = kEipStatusError;
  } else if (kEipStatusError == NvQosStore(&g_qos)) {
    eip_status = kEipStatusError;
  }

  CipQosUpdateUsedSetQosValues();
  return eip_status;
}

EipStatus NvQosSetCallback(CipInstance *const instance,
                           CipAttributeStruct *const attribute,
                           CipByte service) {
  (void)service;
#ifndef OPENER_WITH_TRACES
  (void)instance;
#endif

  EipStatus status = kEipStatusOk;

  if (0 != (kNvDataFunc & attribute->attribute_flags)) {
    OPENER_TRACE_INFO("NV data update: %s, i %" PRIu32 ", a %" PRIu16 "\n",
                      instance->cip_class->class_name,
                      instance->instance_number,
                      attribute->attribute_number);
    status = NvQosStore(&g_qos);
  }
  return status;
}

EipStatus NvTcpipSetCallback(CipInstance *const instance,
                             CipAttributeStruct *const attribute,
                             CipByte service) {
#ifndef OPENER_WITH_TRACES
  (void)instance;
#endif

  EipStatus status = kEipStatusOk;

  if (0 != (kNvDataFunc & attribute->attribute_flags)) {
    if (0 == (0x80U & service)) {
      OPENER_TRACE_INFO("NV data update: %s, i %" PRIu32 ", a %" PRIu16 "\n",
                        instance->cip_class->class_name,
                        instance->instance_number,
                        attribute->attribute_number);
      status = NvTcpipStore(&g_tcpip);
    }
  }
  return status;
}
