/*
 * gateway_storage.c — Mount boot CF/IDE FAT volume for config persistence.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_storage.h"

#include "gateway_config.h"
#include "leap_time.h"
#include "leapos_ich7_sata.h"

#include <rtems/bdbuf.h>
#include <rtems/bdpart.h>
#include <rtems/fsmount.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LEAP_GATEWAY_STORAGE_RETRY_COUNT 10
#define LEAP_GATEWAY_STORAGE_RETRY_SEC   1

static int         g_storage_ready;
static int         g_bdbuf_ready;
static const char* g_storage_mount_point = LEAP_GATEWAY_STORAGE_MOUNT;
static char        g_storage_source[16];

static int
gateway_try_mount_source(const char* source)
{
    const fstab_t tab[] = {
        {
            source,
            LEAP_GATEWAY_STORAGE_MOUNT,
            "dosfs",
            RTEMS_FILESYSTEM_READ_WRITE,
            FSMOUNT_MNT_OK | FSMOUNT_MNTPNT_CRTERR | FSMOUNT_MNT_FAILED,
            0
        }
    };

    return (rtems_fsmount(tab, sizeof(tab) / sizeof(tab[0]), NULL) == 0) ? 0 : -1;
}

static int
gateway_try_register_disk(const char* disk_path)
{
    rtems_status_code sc;
    int                 attempt;

    for (attempt = 0; attempt < LEAP_GATEWAY_STORAGE_RETRY_COUNT; ++attempt)
    {
        sc = rtems_bdpart_register_from_disk(disk_path);
        if (sc == RTEMS_SUCCESSFUL)
        {
            printf(
                LEAP_TS_FMT LEAP_ANSI_OK "Storage: registered %s partition table" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                disk_path);
            return 0;
        }

        if (attempt + 1 < LEAP_GATEWAY_STORAGE_RETRY_COUNT)
        {
            printf(
                LEAP_TS_FMT LEAP_ANSI_INFO
                "Storage: waiting for %s (%s, retry %d/%d)" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                disk_path,
                rtems_status_text(sc),
                attempt + 2,
                LEAP_GATEWAY_STORAGE_RETRY_COUNT);
            sleep(LEAP_GATEWAY_STORAGE_RETRY_SEC);
        }
        else
        {
            printf(
                LEAP_TS_FMT LEAP_ANSI_WARN
                "Storage: %s not readable (%s)" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                disk_path,
                rtems_status_text(sc));
        }
    }

    return -1;
}

static int
gateway_try_disk(const char* disk_path, const char* part_path)
{
    if (gateway_try_register_disk(disk_path) != 0)
    {
        return -1;
    }

    if (gateway_try_mount_source(part_path) == 0)
    {
        strncpy(g_storage_source, part_path, sizeof(g_storage_source) - 1u);
        g_storage_source[sizeof(g_storage_source) - 1u] = '\0';
        return 0;
    }

    printf(
        LEAP_TS_FMT LEAP_ANSI_WARN
        "Storage: FAT mount failed on %s" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str(),
        part_path);

    if (gateway_try_mount_source(disk_path) == 0)
    {
        strncpy(g_storage_source, disk_path, sizeof(g_storage_source) - 1u);
        g_storage_source[sizeof(g_storage_source) - 1u] = '\0';
        return 0;
    }

    printf(
        LEAP_TS_FMT LEAP_ANSI_WARN
        "Storage: FAT mount failed on whole disk %s" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str(),
        disk_path);
    return -1;
}

static int
gateway_mount_boot_volume(void)
{
    static const struct
    {
        const char* disk;
        const char* part;
    } disk_candidates[] = {
        { "/dev/hda", "/dev/hda1" },
        { "/dev/hdb", "/dev/hdb1" },
    };
    size_t i;

    for (i = 0; i < sizeof(disk_candidates) / sizeof(disk_candidates[0]); ++i)
    {
        if (gateway_try_disk(disk_candidates[i].disk, disk_candidates[i].part) == 0)
        {
            g_storage_ready = 1;
            printf(
                LEAP_TS_FMT LEAP_ANSI_OK
                "Storage: config volume mounted %s -> %s" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                g_storage_source,
                g_storage_mount_point);
            return 0;
        }
    }

    return -1;
}

static void
gateway_storage_print_sata_hint(void)
{
    const char* ts = leap_rtems_uptime_str();

    printf(
        LEAP_TS_FMT LEAP_ANSI_WARN
        "Storage: no config volume on /dev/hda or /dev/hdb" LEAP_ANSI_RESET "\n",
        ts);
    printf(
        LEAP_TS_FMT
        "  IDE0 425G + status=00 + read error 7f is a phantom device (garbage IDENTIFY), not your CF"
        LEAP_ANSI_RESET "\n",
        ts);
    printf(
        LEAP_TS_FMT
        "  SATA0 CF: ICH7 prep runs in BSP before IDE0 probe; retry after libbsd if needed"
        LEAP_ANSI_RESET "\n",
        ts);
}

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

    if (g_storage_ready != 0)
    {
        return 0;
    }

    g_storage_source[0] = '\0';

    if (g_bdbuf_ready == 0)
    {
        sc = rtems_bdbuf_init();
        if (sc != RTEMS_SUCCESSFUL)
        {
            printf(
                LEAP_TS_FMT LEAP_ANSI_WARN "Storage: bdbuf init failed: %s" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                rtems_status_text(sc));
            return -1;
        }
        g_bdbuf_ready = 1;
    }

    /* Brief settle after BSP IDE probe before first block read. */
    sleep(1);

    (void)leapos_ich7_sata0_prep();

    if (gateway_mount_boot_volume() == 0)
    {
        return 0;
    }

    gateway_storage_print_sata_hint();
    return -1;
}

int
leap_gateway_storage_retry_after_pci(void)
{
    if (g_storage_ready != 0)
    {
        return 0;
    }

    printf(
        LEAP_TS_FMT LEAP_ANSI_INFO
        "Storage: retrying IDE mount after PCI/libbsd init..." LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());

    sleep(2);

    (void)leapos_ich7_sata0_prep();

    if (gateway_mount_boot_volume() == 0)
    {
        return 0;
    }

    gateway_storage_print_sata_hint();
    return -1;
}
