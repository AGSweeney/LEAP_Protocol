/*
 * gateway_storage_linux.c — Config storage (Linux replacement for gateway_storage.c).
 *
 * On the Alpine image /cf is a plain directory on the (read-write) root
 * filesystem — no separate mount or RTEMS bdbuf/dosfs machinery needed.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_storage.h"

#include "gateway_config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int g_storage_ready;

int
leap_gateway_storage_init(void)
{
    (void)mkdir(LEAP_GATEWAY_STORAGE_MOUNT, 0755);

    if (access(LEAP_GATEWAY_STORAGE_MOUNT, W_OK) == 0)
    {
        g_storage_ready = 1;
        return 0;
    }

    g_storage_ready = 0;
    return -1;
}

int
leap_gateway_storage_retry_after_pci(void)
{
    return leap_gateway_storage_init();
}

int
leap_gateway_storage_ready(void)
{
    return g_storage_ready;
}

const char*
leap_gateway_storage_mount_point(void)
{
    return LEAP_GATEWAY_STORAGE_MOUNT;
}
