/*
 * gateway_storage.c — Mount boot CF/IDE FAT volume for config persistence.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_storage.h"

#include "gateway_config.h"
#include "leap_time.h"

#include <rtems/bdbuf.h>
#include <rtems/bdpart.h>
#include <rtems/fsmount.h>

#include <stdio.h>
#include <string.h>

static int         g_storage_ready;
static const char* g_storage_mount_point = LEAP_GATEWAY_STORAGE_MOUNT;

static fstab_t g_storage_fstab[] = {
    {
        "/dev/hda1",
        LEAP_GATEWAY_STORAGE_MOUNT,
        "dosfs",
        RTEMS_FILESYSTEM_READ_WRITE,
        FSMOUNT_MNT_OK | FSMOUNT_MNTPNT_CRTERR | FSMOUNT_MNT_FAILED,
        0
    },
};

int
leap_gateway_storage_ready(void)
{
    return g_storage_ready;
}

const char*
leap_gateway_storage_mount_point(void)
{
    return g_storage_mount_point;
}

int
leap_gateway_storage_init(void)
{
    rtems_status_code sc;

    g_storage_ready = 0;

    sc = rtems_bdbuf_init();
    if (sc != RTEMS_SUCCESSFUL)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN "Storage: bdbuf init failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            rtems_status_text(sc));
        return -1;
    }

    sc = rtems_bdpart_register_from_disk("/dev/hda");
    if (sc != RTEMS_SUCCESSFUL)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN "Storage: no IDE disk at /dev/hda (%s)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            rtems_status_text(sc));
        return -1;
    }

    sc = rtems_fsmount(
        g_storage_fstab,
        sizeof(g_storage_fstab) / sizeof(g_storage_fstab[0]),
        NULL);
    if (sc != RTEMS_SUCCESSFUL)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN "Storage: FAT mount failed (%s)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            rtems_status_text(sc));
        return -1;
    }

    g_storage_ready = 1;
    printf(
        LEAP_TS_FMT LEAP_ANSI_OK "Storage: config volume mounted at %s" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str(),
        g_storage_mount_point);
    return 0;
}
