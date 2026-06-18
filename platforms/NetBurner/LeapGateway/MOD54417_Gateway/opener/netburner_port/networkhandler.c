/*******************************************************************************
 * NetBurner network handler platform hooks for OpENer.
 ******************************************************************************/

#include "networkhandler.h"

#include "opener_error.h"
#include "trace.h"
#include "opener_user_conf.h"

#include <stdint.h>
#include <constants.h>

extern volatile uint32_t TimeTick;

MicroSeconds GetMicroSeconds(void) {
  return (MicroSeconds)((uint64_t)TimeTick * 1000000ULL / (uint64_t)TICKS_PER_SECOND);
}

MilliSeconds GetMilliSeconds(void) {
  return (MilliSeconds)((uint64_t)TimeTick * 1000ULL / (uint64_t)TICKS_PER_SECOND);
}

EipStatus NetworkHandlerInitializePlatform(void) {
  return kEipStatusOk;
}

void ShutdownSocketPlatform(int socket_handle) {
  if (0 != shutdown(socket_handle, SHUT_RDWR)) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("Failed shutdown() socket %d - Error Code: %d - %s\n",
                     socket_handle,
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
  }
}

void CloseSocketPlatform(int socket_handle) {
  close(socket_handle);
}

int SetSocketToNonBlocking(int socket_handle) {
  (void)socket_handle;
  /* NetBurner TCP/UDP fds are polled via select(); file fcntl does not apply. */
  return 0;
}

int SetQosOnSocket(const int socket, CipUsint qos_value) {
  (void)socket;
  (void)qos_value;
  return 0;
}
