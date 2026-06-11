/*
 * clearcore_leap_identity.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "clearcore_leap_identity.h"

#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_protocol.h"

#include "lwip/netif.h"

#include <string.h>

void clearcore_leap_identity_apply(
    LeapDeviceStack *stack,
    struct netif *   netif)
{
    LeapIdentity *dir_id;
    LeapIdentity *disc_id;
    uint32_t      serial;

    if (stack == NULL || netif == NULL)
    {
        return;
    }

    dir_id  = &stack->dir.config.identity;
    disc_id = &stack->disc.config.identity;
    serial  = ((uint32_t)netif->hwaddr[2] << 24) |
              ((uint32_t)netif->hwaddr[3] << 16) |
              ((uint32_t)netif->hwaddr[4] << 8) |
              (uint32_t)netif->hwaddr[5];

    memcpy(dir_id->primary_mac, netif->hwaddr, 6);
    dir_id->vendor_id         = CLEARCORE_LEAP_VENDOR_ID;
    dir_id->product_code      = CLEARCORE_LEAP_PRODUCT_CODE;
    dir_id->serial_number     = serial;
    dir_id->hardware_revision = 1u;
    dir_id->firmware_revision = CLEARCORE_LEAP_FIRMWARE_REVISION;

    memcpy(disc_id->primary_mac, netif->hwaddr, 6);
    disc_id->vendor_id         = CLEARCORE_LEAP_VENDOR_ID;
    disc_id->product_code      = CLEARCORE_LEAP_PRODUCT_CODE;
    disc_id->serial_number     = serial;
    disc_id->hardware_revision = 1u;
    disc_id->firmware_revision = CLEARCORE_LEAP_FIRMWARE_REVISION;

    leap_dir_device_sync_disc(&stack->dir, &stack->disc);
}
