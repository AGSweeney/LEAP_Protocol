/*******************************************************************************
 * Embedded error helpers — no dynamic allocation.
 ******************************************************************************/

#include "opener_error.h"
#include "opener_net_hal.h"

#include <string.h>

static char s_error_buffer[64];

int GetSocketErrorNumber(void) {
  return OpenerHal_GetSocketError();
}

char *GetErrorMessage(int error_number) {
  const char *message = OpenerHal_GetErrorString(error_number);
  size_t i = 0U;
  while((message[i] != '\0') && (i < (sizeof(s_error_buffer) - 1U))) {
    s_error_buffer[i] = message[i];
    ++i;
  }
  s_error_buffer[i] = '\0';
  return s_error_buffer;
}

void FreeErrorMessage(char *error_message) {
  (void)error_message;
}
