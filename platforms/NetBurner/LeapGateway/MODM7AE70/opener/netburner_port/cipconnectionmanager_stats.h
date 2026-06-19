#ifndef OPENER_CIPCONNECTIONMANAGER_STATS_H_
#define OPENER_CIPCONNECTIONMANAGER_STATS_H_

#include "typedefs.h"
#include "ciptypes.h"

void ConnectionManagerStatsInit(CipClass *connection_manager);

void ConnectionManagerStatsRecordOpenRequest(void);
void ConnectionManagerStatsRecordCloseRequest(void);

void EncodeCipConnectionManagerEntryList(const void *const data,
                                         ENIPMessage *const outgoing_message);

#endif /* OPENER_CIPCONNECTIONMANAGER_STATS_H_ */
