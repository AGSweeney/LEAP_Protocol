/*
 * mcc_pci_dio24h.c — Measurement Computing PCI-DIO-24H userspace scaffold.
 *
 * Hardware model:
 * - PCI vendor/device: ComputerBoards/Measurement Computing 0x1307:0x0014
 * - One Intel 8255-compatible 24-bit DIO block
 * - DIO register BAR: PCI resource index 2 (per Linux Comedi 8255_pci table)
 *
 * This file is intentionally not wired into build-leap-device.sh yet.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "mcc_pci_dio24h.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <unistd.h>

#define MCC_8255_PORT_A   0u
#define MCC_8255_PORT_B   1u
#define MCC_8255_PORT_C   2u
#define MCC_8255_CONTROL  3u
#define MCC_IORESOURCE_IO 0x00000100ul

static int
read_hex_file(const char* path, unsigned long* value_out)
{
    FILE* fp;
    char  buf[64];
    char* endp;

    if (path == NULL || value_out == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return -1;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL)
    {
        fclose(fp);
        errno = EIO;
        return -1;
    }
    fclose(fp);

    errno = 0;
    *value_out = strtoul(buf, &endp, 0);
    if (errno != 0 || endp == buf)
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static int
read_pci_resource(
    const char*   device_path,
    unsigned      bar_index,
    unsigned long* start_out,
    unsigned long* flags_out)
{
    FILE* fp;
    char  path[1024];
    char  line[256];
    unsigned i;
    unsigned long ignored_end;

    if (device_path == NULL || start_out == NULL || flags_out == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    snprintf(path, sizeof(path), "%s/resource", device_path);
    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return -1;
    }

    for (i = 0u; i <= bar_index; ++i)
    {
        if (fgets(line, sizeof(line), fp) == NULL)
        {
            fclose(fp);
            errno = ENOENT;
            return -1;
        }
    }
    fclose(fp);

    if (sscanf(line, "%lx %lx %lx", start_out, &ignored_end, flags_out) != 3)
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static int
find_mcc_pci_dio24h(uint16_t* io_base_out, char* pci_address_out, size_t pci_address_cap)
{
    DIR*           dir;
    struct dirent* ent;
    int            rc = -1;

    if (io_base_out == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    dir = opendir("/sys/bus/pci/devices");
    if (dir == NULL)
    {
        return -1;
    }

    while ((ent = readdir(dir)) != NULL)
    {
        char          device_path[1024];
        char          vendor_path[1024];
        char          device_id_path[1024];
        unsigned long vendor;
        unsigned long device;
        unsigned long bar_start;
        unsigned long bar_flags;

        if (ent->d_name[0] == '.')
        {
            continue;
        }
        if (strlen(ent->d_name) > 128u)
        {
            continue;
        }

        snprintf(device_path, sizeof(device_path), "/sys/bus/pci/devices/%s", ent->d_name);
        snprintf(vendor_path, sizeof(vendor_path), "/sys/bus/pci/devices/%s/vendor", ent->d_name);
        snprintf(device_id_path, sizeof(device_id_path), "/sys/bus/pci/devices/%s/device", ent->d_name);

        if (read_hex_file(vendor_path, &vendor) != 0 ||
            read_hex_file(device_id_path, &device) != 0)
        {
            continue;
        }

        if (vendor != MCC_PCI_DIO24H_VENDOR_ID || device != MCC_PCI_DIO24H_DEVICE_ID)
        {
            continue;
        }

        if (read_pci_resource(
                device_path,
                MCC_PCI_DIO24H_DIO_BAR_INDEX,
                &bar_start,
                &bar_flags) != 0)
        {
            continue;
        }

        if ((bar_flags & MCC_IORESOURCE_IO) == 0ul || bar_start == 0ul || bar_start > 0xfffful)
        {
            errno = ENODEV;
            continue;
        }

        *io_base_out = (uint16_t)bar_start;
        if (pci_address_out != NULL && pci_address_cap > 0u)
        {
            snprintf(pci_address_out, pci_address_cap, "%s", ent->d_name);
        }
        rc = 0;
        break;
    }

    closedir(dir);
    return rc;
}

static inline uint8_t
mcc_in8(uint16_t port)
{
    uint8_t value;

    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void
mcc_out8(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t
make_8255_control_word(
    MccPciDio24hDirection port_a,
    MccPciDio24hDirection port_b,
    MccPciDio24hDirection port_c_low,
    MccPciDio24hDirection port_c_high)
{
    uint8_t control = 0x80u; /* Mode-set, group A/B mode 0. */

    if (port_a == MCC_PCI_DIO24H_DIR_INPUT)
    {
        control |= 0x10u;
    }
    if (port_c_high == MCC_PCI_DIO24H_DIR_INPUT)
    {
        control |= 0x08u;
    }
    if (port_b == MCC_PCI_DIO24H_DIR_INPUT)
    {
        control |= 0x02u;
    }
    if (port_c_low == MCC_PCI_DIO24H_DIR_INPUT)
    {
        control |= 0x01u;
    }

    return control;
}

void
mcc_pci_dio24h_init(MccPciDio24h* dev)
{
    if (dev != NULL)
    {
        memset(dev, 0, sizeof(*dev));
    }
}

int
mcc_pci_dio24h_open(MccPciDio24h* dev, const MccPciDio24hConfig* config)
{
    uint16_t io_base = 0u;

    if (dev == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    mcc_pci_dio24h_init(dev);

    if (config != NULL && config->io_base_override != 0u)
    {
        io_base = config->io_base_override;
        snprintf(dev->pci_address, sizeof(dev->pci_address), "manual:%04x", io_base);
    }
    else if (find_mcc_pci_dio24h(&io_base, dev->pci_address, sizeof(dev->pci_address)) != 0)
    {
        dev->last_errno = errno;
        return -1;
    }

    if (ioperm(io_base, MCC_PCI_DIO24H_IO_SPAN, 1) != 0)
    {
        dev->last_errno = errno;
        return -1;
    }

    dev->io_base = io_base;
    dev->io_ready = 1;
    return mcc_pci_dio24h_configure_all_inputs(dev);
}

void
mcc_pci_dio24h_close(MccPciDio24h* dev)
{
    if (dev == NULL || dev->io_ready == 0)
    {
        return;
    }

    (void)ioperm(dev->io_base, MCC_PCI_DIO24H_IO_SPAN, 0);
    dev->io_ready = 0;
}

int
mcc_pci_dio24h_configure_ports(
    MccPciDio24h*         dev,
    MccPciDio24hDirection port_a,
    MccPciDio24hDirection port_b,
    MccPciDio24hDirection port_c_low,
    MccPciDio24hDirection port_c_high)
{
    uint8_t control;

    if (dev == NULL || dev->io_ready == 0)
    {
        errno = ENODEV;
        return -1;
    }

    control = make_8255_control_word(port_a, port_b, port_c_low, port_c_high);
    mcc_out8((uint16_t)(dev->io_base + MCC_8255_CONTROL), control);
    dev->control_word = control;
    return 0;
}

int
mcc_pci_dio24h_configure_all_inputs(MccPciDio24h* dev)
{
    return mcc_pci_dio24h_configure_ports(
        dev,
        MCC_PCI_DIO24H_DIR_INPUT,
        MCC_PCI_DIO24H_DIR_INPUT,
        MCC_PCI_DIO24H_DIR_INPUT,
        MCC_PCI_DIO24H_DIR_INPUT);
}

int
mcc_pci_dio24h_configure_all_outputs(MccPciDio24h* dev)
{
    return mcc_pci_dio24h_configure_ports(
        dev,
        MCC_PCI_DIO24H_DIR_OUTPUT,
        MCC_PCI_DIO24H_DIR_OUTPUT,
        MCC_PCI_DIO24H_DIR_OUTPUT,
        MCC_PCI_DIO24H_DIR_OUTPUT);
}

int
mcc_pci_dio24h_write_port(MccPciDio24h* dev, MccPciDio24hPort port, uint8_t value)
{
    if (dev == NULL || dev->io_ready == 0 || port > MCC_PCI_DIO24H_PORT_C)
    {
        errno = EINVAL;
        return -1;
    }

    mcc_out8((uint16_t)(dev->io_base + (uint16_t)port), value);
    dev->output_shadow[port] = value;
    return 0;
}

int
mcc_pci_dio24h_read_port(MccPciDio24h* dev, MccPciDio24hPort port, uint8_t* value_out)
{
    if (dev == NULL || dev->io_ready == 0 || value_out == NULL || port > MCC_PCI_DIO24H_PORT_C)
    {
        errno = EINVAL;
        return -1;
    }

    *value_out = mcc_in8((uint16_t)(dev->io_base + (uint16_t)port));
    return 0;
}

int
mcc_pci_dio24h_write_24(MccPciDio24h* dev, uint32_t value)
{
    if (value > 0x00ffffffu)
    {
        errno = ERANGE;
        return -1;
    }

    if (mcc_pci_dio24h_write_port(dev, MCC_PCI_DIO24H_PORT_A, (uint8_t)(value & 0xffu)) != 0)
    {
        return -1;
    }
    if (mcc_pci_dio24h_write_port(dev, MCC_PCI_DIO24H_PORT_B, (uint8_t)((value >> 8) & 0xffu)) != 0)
    {
        return -1;
    }
    return mcc_pci_dio24h_write_port(dev, MCC_PCI_DIO24H_PORT_C, (uint8_t)((value >> 16) & 0xffu));
}

int
mcc_pci_dio24h_read_24(MccPciDio24h* dev, uint32_t* value_out)
{
    uint8_t port_a;
    uint8_t port_b;
    uint8_t port_c;

    if (value_out == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (mcc_pci_dio24h_read_port(dev, MCC_PCI_DIO24H_PORT_A, &port_a) != 0 ||
        mcc_pci_dio24h_read_port(dev, MCC_PCI_DIO24H_PORT_B, &port_b) != 0 ||
        mcc_pci_dio24h_read_port(dev, MCC_PCI_DIO24H_PORT_C, &port_c) != 0)
    {
        return -1;
    }

    *value_out = (uint32_t)port_a | ((uint32_t)port_b << 8) | ((uint32_t)port_c << 16);
    return 0;
}
