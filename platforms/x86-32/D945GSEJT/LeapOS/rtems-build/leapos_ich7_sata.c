/*
 * leapos_ich7_sata.c — ICH7/ICH7-M SATA port 0 legacy IDE prep for D945GSEJT.
 *
 * Program ICH7 SATA (PCI 00:1f.2) for legacy primary-master routing before
 * the pc386 IDE driver probes 0x1F0. Conservative PCI writes only — avoid
 * SCLKCG/PATA stomp that hung early IDE probe on D945GSEJT.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leapos_ich7_sata.h"

#include <stdint.h>
#include <rtems/bspIo.h>

#define PCI_CFG_ADDR 0x0cf8u
#define PCI_CFG_DATA 0x0cfcu

#define PCI_VENDOR_INTEL 0x8086u

#define ICH7_SATA_BUS 0u
#define ICH7_SATA_DEV 31u
#define ICH7_PATA_FN  1u
#define ICH7_SATA_FN  2u

#define PCI_REG_COMMAND     0x04u
#define ICH7_REG_MAP        0x90u
#define ICH7_REG_PCS        0x92u
#define ICH7_REG_IDE_TIM_P  0x40u
#define ICH7_REG_IDE_TIM_S  0x42u

#define ICH7_MAP_AHCI_EN    (1u << 6)
#define ICH7_PCS_SATA_EN    (1u << 15)
#define ICH7_IDE_DECODE_EN  (1u << 15)

#define ICH7_SATA_PORT0     0x01u

static inline void
leapos_io_outl(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t
leapos_io_inl(uint16_t port)
{
    uint32_t value;

    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t
leapos_pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg)
{
    uint32_t addr =
        0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)dev << 11) |
        ((uint32_t)fn << 8) |
        ((uint32_t)reg & 0xfcu);

    leapos_io_outl(PCI_CFG_ADDR, addr);
    return leapos_io_inl(PCI_CFG_DATA);
}

static void
leapos_pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg, uint32_t value)
{
    uint32_t addr =
        0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)dev << 11) |
        ((uint32_t)fn << 8) |
        ((uint32_t)reg & 0xfcu);

    leapos_io_outl(PCI_CFG_ADDR, addr);
    leapos_io_outl(PCI_CFG_DATA, value);
}

static uint16_t
leapos_pci_read16(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg)
{
    uint32_t value = leapos_pci_read32(bus, dev, fn, reg);

    if (reg & 2u)
    {
        return (uint16_t)((value >> 16) & 0xffffu);
    }

    return (uint16_t)(value & 0xffffu);
}

static void
leapos_pci_write16(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg, uint16_t value)
{
    uint32_t current = leapos_pci_read32(bus, dev, fn, reg);

    if (reg & 2u)
    {
        current = (current & 0x0000ffffu) | ((uint32_t)value << 16);
    }
    else
    {
        current = (current & 0xffff0000u) | value;
    }

    leapos_pci_write32(bus, dev, fn, reg & 0xfcu, current);
}

static void
leapos_ich7_pata_legacy_disable(void)
{
    uint16_t timing;

    if (leapos_pci_read16(ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_PATA_FN, 0x00u) !=
        PCI_VENDOR_INTEL)
    {
        return;
    }

    timing = leapos_pci_read16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_PATA_FN, ICH7_REG_IDE_TIM_P);
    timing &= (uint16_t)~ICH7_IDE_DECODE_EN;
    leapos_pci_write16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_PATA_FN, ICH7_REG_IDE_TIM_P, timing);

    timing = leapos_pci_read16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_PATA_FN, ICH7_REG_IDE_TIM_S);
    timing &= (uint16_t)~ICH7_IDE_DECODE_EN;
    leapos_pci_write16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_PATA_FN, ICH7_REG_IDE_TIM_S, timing);
}

int
leapos_ich7_sata0_prep(void)
{
    uint32_t id;
    uint16_t vendor;
    uint16_t device;
    uint16_t command;
    uint16_t map;
    uint16_t pcs;
    uint16_t ide_tim;

    id = leapos_pci_read32(ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, 0x00u);
    vendor = (uint16_t)(id & 0xffffu);
    device = (uint16_t)((id >> 16) & 0xffffu);

    if (vendor != PCI_VENDOR_INTEL)
    {
        return -1;
    }

    if (device < 0x27c0u || device > 0x27c6u)
    {
        return -1;
    }

    leapos_ich7_pata_legacy_disable();

    command = leapos_pci_read16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, PCI_REG_COMMAND);
    command = (uint16_t)(command | 0x0005u);
    leapos_pci_write16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, PCI_REG_COMMAND, command);

    map = leapos_pci_read16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_MAP);
    if ((map & ICH7_MAP_AHCI_EN) != 0u)
    {
        map = (uint16_t)(map & ~ICH7_MAP_AHCI_EN);
        leapos_pci_write16(
            ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_MAP, map);
    }

    pcs = leapos_pci_read16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_PCS);
    pcs = (uint16_t)(pcs | ICH7_PCS_SATA_EN | ICH7_SATA_PORT0);
    leapos_pci_write16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_PCS, pcs);

    ide_tim = leapos_pci_read16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_IDE_TIM_P);
    ide_tim = (uint16_t)(ide_tim | ICH7_IDE_DECODE_EN);
    leapos_pci_write16(
        ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_IDE_TIM_P, ide_tim);

    printk(
        "ICH7: SATA0 legacy prep (dev %04x MAP=%04x PCS=%04x)\n",
        device,
        leapos_pci_read16(
            ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_MAP),
        leapos_pci_read16(
            ICH7_SATA_BUS, ICH7_SATA_DEV, ICH7_SATA_FN, ICH7_REG_PCS));
    return 0;
}
