/*******************************************************************************
 * OpENer_uC-NetBurner — LLDP CIP objects (0x109 / 0x10A) wired to opener_nb_lldp
 *
 * Built when OPENER_NB_LLDP=ON. GetAttribute handlers call OpenerNbLldpBuild*;
 * SetAttribute on management object calls OpenerNbLldpSetMgmtAttr. Neighbor table
 * instances are dynamic (up to OPENER_LLDP_DT_MAX_INSTANCES).
 ******************************************************************************/
#include "ciplldpmanagement.h"
#include "ciplldpdatatable.h"

#include "cipcommon.h"
#include "ciperror.h"
#include "endianconv.h"
#include "opener_api.h"
#include "trace.h"

#include <string.h>

#if defined(OPENER_NB_LLDP) && OPENER_NB_LLDP

#include "opener_nb_lldp.h"
#include "opener_nb_platform_types.h"

#define OPENER_LLDP_DT_MAX_INSTANCES 48u
#define OPENER_LLDP_ENCODE_BUF_SIZE  128u

static CipInstance *s_lldp_encode_instance = NULL;

typedef struct {
  CipUsint attribute_number;
} OpenerLldpAttrRef;

static void AppendRawToMessage(const uint8_t *data, size_t len,
                               ENIPMessage *const outgoing_message) {
  if((NULL == data) || (NULL == outgoing_message) || (0U == len)) {
    return;
  }
  memcpy(outgoing_message->current_message_position, data, len);
  outgoing_message->current_message_position += len;
  outgoing_message->used_message_length += len;
}

static void LldpPreGetCallback(CipInstance *const instance,
                               CipAttributeStruct *const attribute,
                               CipByte service) {
  (void)attribute;
  (void)service;
  s_lldp_encode_instance = instance;
}

static CipUint s_lldp_dt_class_max_instance;
static CipUint s_lldp_dt_class_instance_count;

static void EncodeLldpDataTableClassMaxInstance(const void *const data,
                                                ENIPMessage *const outgoing_message) {
  (void)data;
  s_lldp_dt_class_max_instance = (CipUint)OpenerNbLldpGetDataTableMaxInstance();
  EncodeCipUint(&s_lldp_dt_class_max_instance, outgoing_message);
}

static void EncodeLldpDataTableClassInstanceCount(const void *const data,
                                                  ENIPMessage *const outgoing_message) {
  (void)data;
  s_lldp_dt_class_instance_count = (CipUint)OpenerNbLldpGetNeighborCount();
  EncodeCipUint(&s_lldp_dt_class_instance_count, outgoing_message);
}

static void InitializeLldpDataTableClass(CipClass *class) {
  CipClass *meta_class = class->class_instance.cip_class;

  InsertAttribute((CipInstance *)class, 1, kCipUint, EncodeCipUint, NULL,
                  (void *)&class->revision, kGetableSingleAndAll);
  InsertAttribute((CipInstance *)class, 2, kCipUint,
                  EncodeLldpDataTableClassMaxInstance, NULL,
                  (void *)&class->max_instance, kGetableSingleAndAll);
  InsertAttribute((CipInstance *)class, 3, kCipUint,
                  EncodeLldpDataTableClassInstanceCount, NULL,
                  (void *)&class->number_of_instances, kGetableSingleAndAll);
  InsertAttribute((CipInstance *)class, 4, kCipUint, EncodeCipUint, NULL,
                  (void *)&kCipUintZero, kGetableAllDummy);
  InsertAttribute((CipInstance *)class, 5, kCipUint, EncodeCipUint, NULL,
                  (void *)&kCipUintZero, kNotSetOrGetable);
  InsertAttribute((CipInstance *)class, 6, kCipUint, EncodeCipUint, NULL,
                  (void *)&meta_class->highest_attribute_number, kGetableSingle);
  InsertAttribute((CipInstance *)class, 7, kCipUint, EncodeCipUint, NULL,
                  (void *)&class->highest_attribute_number, kGetableSingle);

  if(meta_class->number_of_services > 1) {
    InsertService(meta_class, kGetAttributeAll, &GetAttributeAll, "GetAttributeAll");
  }
  InsertService(meta_class, kGetAttributeSingle, &GetAttributeSingle,
                "GetAttributeSingle");
}

static void EncodeLldpMgmtAttribute(const void *const data,
                                    ENIPMessage *const outgoing_message) {
  const OpenerLldpAttrRef *ref = (const OpenerLldpAttrRef *)data;
  uint8_t buffer[OPENER_LLDP_ENCODE_BUF_SIZE];
  size_t length = 0U;

  if((NULL == ref) || (NULL == outgoing_message)) {
    return;
  }
  if(OpenerNbLldpBuildMgmtAttr(ref->attribute_number, buffer, sizeof(buffer),
                               &length)) {
    AppendRawToMessage(buffer, length, outgoing_message);
  }
}

static void EncodeLldpDataTableAttribute(const void *const data,
                                         ENIPMessage *const outgoing_message) {
  const OpenerLldpAttrRef *ref = (const OpenerLldpAttrRef *)data;
  uint8_t buffer[OPENER_LLDP_ENCODE_BUF_SIZE];
  size_t length = 0U;
  CipUint instance_number = 0U;

  if((NULL == ref) || (NULL == outgoing_message) || (NULL == s_lldp_encode_instance)) {
    return;
  }

  instance_number = s_lldp_encode_instance->instance_number;
  if(!OpenerNbLldpDataTableInstanceValid((uint16_t)instance_number)) {
    return;
  }

  if(OpenerNbLldpBuildDataTableAttr((uint16_t)instance_number, ref->attribute_number,
                                     buffer, sizeof(buffer), &length)) {
    AppendRawToMessage(buffer, length, outgoing_message);
  }
}

static int DecodeLldpMgmtAttribute(void *const data,
                                   CipMessageRouterRequest *const message_router_request,
                                   CipMessageRouterResponse *const message_router_response) {
  const OpenerLldpAttrRef *ref = (const OpenerLldpAttrRef *)data;
  opener_nb_status_t status = OPENER_NB_ERR_NOT_SUPPORTED;
  uint8_t cip_status = kCipErrorAttributeNotSetable;

  (void)ref;
  if((NULL == ref) || (NULL == message_router_request) ||
     (NULL == message_router_response)) {
    return -1;
  }

  status = OpenerNbLldpSetMgmtAttr(ref->attribute_number,
                                   message_router_request->data,
                                   message_router_request->request_data_size,
                                   &cip_status);
  if(OPENER_NB_OK == status) {
    message_router_response->general_status = kCipErrorSuccess;
    return (int)message_router_request->request_data_size;
  }

  message_router_response->general_status = cip_status;
  return -1;
}

static OpenerLldpAttrRef s_mgmt_attr_refs[5];
static OpenerLldpAttrRef s_dt_attr_refs[OPENER_LLDP_DT_MAX_INSTANCES][9];

static void InitLldpAttributeRefs(void) {
  CipUsint attr = 0U;
  for(attr = 1U; attr <= 5U; ++attr) {
    s_mgmt_attr_refs[attr - 1U].attribute_number = attr;
  }
  for(CipUint inst = 0U; inst < OPENER_LLDP_DT_MAX_INSTANCES; ++inst) {
    for(attr = 0U; attr < 9U; ++attr) {
      s_dt_attr_refs[inst][attr].attribute_number = (CipUsint)(attr + 1U);
    }
  }
}

EipStatus CipLldpManagementInit(void) {
  InitLldpAttributeRefs();

  CipClass *lldp_management_class = CreateCipClass(
    kCipLldpManagementClassCode,
    7,
    7,
    2,
    5,
    5,
    2,
    1,
    "LLDP Management",
    1,
    NULL);
  if(NULL == lldp_management_class) {
    return kEipStatusError;
  }

  lldp_management_class->PreGetCallback = LldpPreGetCallback;

  CipInstance *instance = GetCipInstance(lldp_management_class, 1);
  if(NULL == instance) {
    return kEipStatusError;
  }

  InsertAttribute(instance, 1, kCipAny, EncodeLldpMgmtAttribute, DecodeLldpMgmtAttribute,
                  &s_mgmt_attr_refs[0], kSetAndGetAble);
  InsertAttribute(instance, 2, kCipAny, EncodeLldpMgmtAttribute, DecodeLldpMgmtAttribute,
                  &s_mgmt_attr_refs[1], kSetAndGetAble);
  InsertAttribute(instance, 3, kCipAny, EncodeLldpMgmtAttribute, DecodeLldpMgmtAttribute,
                  &s_mgmt_attr_refs[2], kSetAndGetAble);
  InsertAttribute(instance, 4, kCipAny, EncodeLldpMgmtAttribute, NULL,
                  &s_mgmt_attr_refs[3], kGetableSingleAndAll);
  InsertAttribute(instance, 5, kCipAny, EncodeLldpMgmtAttribute, NULL,
                  &s_mgmt_attr_refs[4], kGetableSingleAndAll);

  InsertService(lldp_management_class, kGetAttributeSingle, &GetAttributeSingle,
                "GetAttributeSingle");
  InsertService(lldp_management_class, kGetAttributeAll, &GetAttributeAll,
                "GetAttributeAll");
  InsertService(lldp_management_class, kSetAttributeSingle, &SetAttributeSingle,
                "SetAttributeSingle");

  return kEipStatusOk;
}

EipStatus CipLldpDataTableInit(void) {
  InitLldpAttributeRefs();

  CipClass *lldp_data_table_class = CreateCipClass(
    kCipLldpDataTableClassCode,
    7,
    7,
    2,
    9,
    9,
    2,
    OPENER_LLDP_DT_MAX_INSTANCES,
    "LLDP Data Table",
    1,
    InitializeLldpDataTableClass);
  if(NULL == lldp_data_table_class) {
    return kEipStatusError;
  }

  lldp_data_table_class->PreGetCallback = LldpPreGetCallback;

  for(CipUint inst_num = 1U; inst_num <= OPENER_LLDP_DT_MAX_INSTANCES; ++inst_num) {
    CipInstance *instance = GetCipInstance(lldp_data_table_class, inst_num);
    if(NULL == instance) {
      return kEipStatusError;
    }

    for(CipUsint attr_idx = 0U; attr_idx < 9U; ++attr_idx) {
      InsertAttribute(instance,
                      (CipUsint)(attr_idx + 1U),
                      kCipAny,
                      EncodeLldpDataTableAttribute,
                      NULL,
                      &s_dt_attr_refs[inst_num - 1U][attr_idx],
                      kGetableSingleAndAll);
    }
  }

  InsertService(lldp_data_table_class, kGetAttributeSingle, &GetAttributeSingle,
                "GetAttributeSingle");
  InsertService(lldp_data_table_class, kGetAttributeAll, &GetAttributeAll,
                "GetAttributeAll");

  return kEipStatusOk;
}

#endif /* OPENER_NB_LLDP */
