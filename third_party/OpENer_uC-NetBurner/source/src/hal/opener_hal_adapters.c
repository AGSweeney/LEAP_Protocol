/*******************************************************************************
 * Legacy network-handler adapter — bridges old port hooks to the embedded HAL.
 ******************************************************************************/

#include "networkhandler.h"

#include "opener_timer_hal.h"
#include "opener_net_hal.h"

EipStatus NetworkHandlerInitializePlatform(void) {
  return (OpenerHal_TimerInit() == kOpenerHalOk) ? kEipStatusOk : kEipStatusError;
}

void ShutdownSocketPlatform(int socket_handle) {
  OpenerHal_SocketShutdown(socket_handle);
}

void CloseSocketPlatform(int socket_handle) {
  OpenerHal_SocketClose(socket_handle);
}

int SetSocketToNonBlocking(int socket_handle) {
  return (OpenerHal_SocketSetNonBlocking(socket_handle) == kOpenerHalOk) ? 0 : -1;
}

MicroSeconds GetMicroSeconds(void) {
  return OpenerHal_TimerGetMicroseconds();
}

MilliSeconds GetMilliSeconds(void) {
  return OpenerHal_TimerGetMilliseconds();
}

int SetQosOnSocket(const int socket, CipUsint qos_value) {
  return (OpenerHal_SocketSetQoS(socket, qos_value) == kOpenerHalOk) ? 0 : -1;
}
