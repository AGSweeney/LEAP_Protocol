#include "opener_nb_fault.h"

#include "appcontype.h"
#include "opener_api.h"

void OpenerNbAcdOnFault(void) {
  CloseAllConnections();
}
