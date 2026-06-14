/*
 * mcc_pci_dio24h.h — Measurement Computing PCI-DIO-24H userspace scaffold.
 *
 * Not wired into the LeapDevice-linux build yet. This API exposes the full
 * 24-channel 8255 digital I/O surface so the LEAP profile mapping can be chosen
 * when the hardware is in the lab.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_MCC_PCI_DIO24H_H
#define LEAP_MCC_PCI_DIO24H_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCC_PCI_DIO24H_VENDOR_ID 0x1307u
#define MCC_PCI_DIO24H_DEVICE_ID 0x0014u

/* Comedi's 8255_pci table identifies the DIO 8255 register BAR as index 2. */
#define MCC_PCI_DIO24H_DIO_BAR_INDEX 2u
#define MCC_PCI_DIO24H_IO_SPAN       4u
#define MCC_PCI_DIO24H_CHANNELS      24u

typedef enum MccPciDio24hPort
{
    MCC_PCI_DIO24H_PORT_A = 0,
    MCC_PCI_DIO24H_PORT_B = 1,
    MCC_PCI_DIO24H_PORT_C = 2
} MccPciDio24hPort;

typedef enum MccPciDio24hDirection
{
    MCC_PCI_DIO24H_DIR_OUTPUT = 0,
    MCC_PCI_DIO24H_DIR_INPUT  = 1
} MccPciDio24hDirection;

typedef struct MccPciDio24hConfig
{
    /*
     * Optional fixed I/O base override for first hardware bring-up. When zero,
     * open() scans /sys/bus/pci/devices for vendor 0x1307/device 0x0014 and
     * reads BAR 2 from its resource file.
     */
    uint16_t io_base_override;
} MccPciDio24hConfig;

typedef struct MccPciDio24h
{
    uint16_t io_base;
    uint8_t  control_word;
    uint8_t  output_shadow[3];
    int      io_ready;
    int      last_errno;
    char     pci_address[32];
} MccPciDio24h;

void mcc_pci_dio24h_init(MccPciDio24h* dev);

int mcc_pci_dio24h_open(
    MccPciDio24h*             dev,
    const MccPciDio24hConfig* config);

void mcc_pci_dio24h_close(MccPciDio24h* dev);

int mcc_pci_dio24h_configure_ports(
    MccPciDio24h*           dev,
    MccPciDio24hDirection   port_a,
    MccPciDio24hDirection   port_b,
    MccPciDio24hDirection   port_c_low,
    MccPciDio24hDirection   port_c_high);

int mcc_pci_dio24h_configure_all_inputs(MccPciDio24h* dev);
int mcc_pci_dio24h_configure_all_outputs(MccPciDio24h* dev);

int mcc_pci_dio24h_write_port(MccPciDio24h* dev, MccPciDio24hPort port, uint8_t value);
int mcc_pci_dio24h_read_port(MccPciDio24h* dev, MccPciDio24hPort port, uint8_t* value_out);

int mcc_pci_dio24h_write_24(MccPciDio24h* dev, uint32_t value);
int mcc_pci_dio24h_read_24(MccPciDio24h* dev, uint32_t* value_out);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_MCC_PCI_DIO24H_H */
