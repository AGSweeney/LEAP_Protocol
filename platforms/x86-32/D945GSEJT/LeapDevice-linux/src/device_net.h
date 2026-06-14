/*
 * device_net.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DEVICE_NET_H
#define LEAP_DEVICE_NET_H

int device_net_wait_for_iface(const char* ifname, int timeout_s);
int device_net_bring_up(const char* ifname);

#endif /* LEAP_DEVICE_NET_H */
