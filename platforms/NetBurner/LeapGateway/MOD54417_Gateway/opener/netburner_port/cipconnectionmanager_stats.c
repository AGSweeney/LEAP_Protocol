/******************************************************************************
 * Connection Manager instance attributes for ODVA / Molex CT tooling.
 *
 * OpENer upstream registers class-level meta attributes only; instance 1
 * attributes (Open Requests, Connection Entry List, etc.) must be added here.
 ******************************************************************************/

#include "cipconnectionmanager_stats.h"

#include <string.h>

#include "cipcommon.h"
#include "cipconnectionobject.h"
#include "doublylinkedlist.h"
#include "enipmessage.h"
#include "endianconv.h"
#include "opener_user_conf.h"

typedef struct {
  CipUint open_requests;
  CipUint open_format_rejects;
  CipUint open_resource_rejects;
  CipUint open_other_rejects;
  CipUint close_requests;
  CipUint close_format_rejects;
  CipUint close_other_rejects;
  CipUint connection_timeouts;
  CipUint cpu_utilization;
  CipUdint max_buff_size;
  CipUdint bufsize_remaining;
  CipConnectionManagerConnectionEntryList connection_entry_list;
} ConnectionManagerStats;

static ConnectionManagerStats g_connection_manager_stats;
static CipBool g_conn_open_bits[OPENER_NUMBER_OF_SUPPORTED_SESSIONS];

static void RefreshConnectionEntryList(void) {
  const CipUint max_entries = (CipUint)OPENER_NUMBER_OF_SUPPORTED_SESSIONS;
  CipUint active_count = 0U;
  DoublyLinkedListNode *node = connection_list.first;

  memset(g_conn_open_bits, 0, sizeof(g_conn_open_bits));

  while (node != NULL) {
    const CipConnectionObject *const connection_object = node->data;
    const CipUint slot =
      (CipUint)(connection_object->connection_number % max_entries);

    g_conn_open_bits[slot] = 1U;
    ++active_count;
    node = node->next;
  }

  g_connection_manager_stats.connection_entry_list.num_conn_entries = max_entries;
  g_connection_manager_stats.connection_entry_list.conn_open_bits = g_conn_open_bits;
  (void)active_count;
}

void EncodeCipConnectionManagerEntryList(const void *const data,
                                         ENIPMessage *const outgoing_message) {
  const CipConnectionManagerConnectionEntryList *const entry_list =
    (const CipConnectionManagerConnectionEntryList *)data;
  const CipUint entry_count = entry_list->num_conn_entries;
  const size_t byte_count = (size_t)((entry_count + 7U) / 8U);

  RefreshConnectionEntryList();
  AddIntToMessage(g_connection_manager_stats.connection_entry_list.num_conn_entries,
                  outgoing_message);

  if ((byte_count > 0U) && (entry_list->conn_open_bits != NULL)) {
    memcpy(outgoing_message->current_message_position,
           entry_list->conn_open_bits,
           byte_count);
    outgoing_message->current_message_position += byte_count;
    outgoing_message->used_message_length += byte_count;
  }
}

void ConnectionManagerStatsRecordOpenRequest(void) {
  ++g_connection_manager_stats.open_requests;
}

void ConnectionManagerStatsRecordCloseRequest(void) {
  ++g_connection_manager_stats.close_requests;
}

void ConnectionManagerStatsInit(CipClass *connection_manager) {
  CipInstance *instance = GetCipInstance(connection_manager, 1);

  if (instance == NULL) {
    return;
  }

  memset(&g_connection_manager_stats, 0, sizeof(g_connection_manager_stats));
  g_connection_manager_stats.connection_entry_list.num_conn_entries =
    (CipUint)OPENER_NUMBER_OF_SUPPORTED_SESSIONS;
  g_connection_manager_stats.connection_entry_list.conn_open_bits = g_conn_open_bits;

  InsertAttribute(instance, 1, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.open_requests,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 2, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.open_format_rejects,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 3, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.open_resource_rejects,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 4, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.open_other_rejects,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 5, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.close_requests,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 6, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.close_format_rejects,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 7, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.close_other_rejects,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 8, kCipUint, EncodeCipUint, DecodeCipUint,
                  &g_connection_manager_stats.connection_timeouts,
                  kGetableSingleAndAll | kSetable);
  InsertAttribute(instance, 9, kCipByteArray, EncodeCipConnectionManagerEntryList,
                  NULL, &g_connection_manager_stats.connection_entry_list,
                  kGetableSingleAndAll);
  InsertAttribute(instance, 11, kCipUint, EncodeCipUint, NULL,
                  &g_connection_manager_stats.cpu_utilization,
                  kGetableSingleAndAll);
  InsertAttribute(instance, 12, kCipUdint, EncodeCipUdint, NULL,
                  &g_connection_manager_stats.max_buff_size,
                  kGetableSingleAndAll);
  InsertAttribute(instance, 13, kCipUdint, EncodeCipUdint, NULL,
                  &g_connection_manager_stats.bufsize_remaining,
                  kGetableSingleAndAll);
}
