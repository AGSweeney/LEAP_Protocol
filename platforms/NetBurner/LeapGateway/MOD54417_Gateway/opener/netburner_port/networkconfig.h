/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 ******************************************************************************/

#ifndef OPENER_NETWORKCONFIG_H_
#define OPENER_NETWORKCONFIG_H_

#include "typedefs.h"
#include "ciptcpipinterface.h"

EipStatus IfaceGetMacAddress(const char *iface,
                             uint8_t *const physical_address);

EipStatus IfaceGetConfiguration(const char *iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg);

EipStatus IfaceWaitForIp(const char *const iface,
                         int timeout,
                         volatile int *const abort_wait);

void GetHostName(CipString *hostname);

#endif /* OPENER_NETWORKCONFIG_H_ */
