/*
 * leap_board_mcc_dio24.c — MCC PCI-DIO-24H digital I/O for Alpine LEAP device.
 *
 * 8255 mode 0 layout for LEAP 16x16 profile:
 *   Port A (8) + Port B (8) = 16 outputs (LEAP bits 0..15)
 *   Port C (8)              = 8 inputs  (LEAP bits 0..7)
 *
 * Optional env MCC_DIO24_IO_BASE=<hex> for manual I/O base when PCI scan fails.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_board.h"

#include "mcc_pci_dio24h.h"
#include "leap/leap_protocol.h"

#include <stdlib.h>
#include <string.h>

static MccPciDio24h g_mcc;

static int
leap_board_parse_io_base_override(uint16_t* io_base_out)
{
    const char* env;

    if (io_base_out == NULL)
    {
        return 0;
    }

    env = getenv("MCC_DIO24_IO_BASE");
    if (env == NULL || env[0] == '\0')
    {
        return 0;
    }

    *io_base_out = (uint16_t)strtoul(env, NULL, 0);
    return (*io_base_out != 0u) ? 1 : 0;
}

const char*
leap_board_description(void)
{
    return "MCC PCI-DIO-24 16x8 I/O";
}

void
leap_rtems_board_init(LeapRtemsBoardIo* io)
{
    MccPciDio24hConfig config;
    uint16_t             io_override = 0u;

    if (io == NULL)
    {
        return;
    }

    memset(io, 0, sizeof(*io));
    mcc_pci_dio24h_init(&g_mcc);

    memset(&config, 0, sizeof(config));
    if (leap_board_parse_io_base_override(&io_override) != 0)
    {
        config.io_base_override = io_override;
    }

    if (mcc_pci_dio24h_open(&g_mcc, &config) != 0)
    {
        io->io_status = LEAP_DIO_STATUS_OUTPUT_SHORT;
        return;
    }

    if (mcc_pci_dio24h_configure_ports(
            &g_mcc,
            MCC_PCI_DIO24H_DIR_OUTPUT,
            MCC_PCI_DIO24H_DIR_OUTPUT,
            MCC_PCI_DIO24H_DIR_INPUT,
            MCC_PCI_DIO24H_DIR_INPUT) != 0)
    {
        io->io_status = LEAP_DIO_STATUS_OUTPUT_SHORT;
        return;
    }

    (void)mcc_pci_dio24h_write_port(&g_mcc, MCC_PCI_DIO24H_PORT_A, 0u);
    (void)mcc_pci_dio24h_write_port(&g_mcc, MCC_PCI_DIO24H_PORT_B, 0u);
    io->io_status = LEAP_DIO_STATUS_OK;
}

void
leap_rtems_board_apply_outputs(LeapRtemsBoardIo* io, uint16_t outputs)
{
    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = outputs;

    if (g_mcc.io_ready == 0)
    {
        return;
    }

    (void)mcc_pci_dio24h_write_port(&g_mcc, MCC_PCI_DIO24H_PORT_A, (uint8_t)(outputs & 0xffu));
    (void)mcc_pci_dio24h_write_port(
        &g_mcc, MCC_PCI_DIO24H_PORT_B, (uint8_t)((outputs >> 8) & 0xffu));
}

void
leap_rtems_board_enter_safe(LeapRtemsBoardIo* io)
{
    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = 0u;
    leap_rtems_board_apply_outputs(io, 0u);
}

void
leap_rtems_board_sample_inputs(LeapRtemsBoardIo* io)
{
    uint8_t port_c = 0u;

    if (io == NULL)
    {
        return;
    }

    if (g_mcc.io_ready == 0)
    {
        io->digital_inputs = 0u;
        return;
    }

    if (mcc_pci_dio24h_read_port(&g_mcc, MCC_PCI_DIO24H_PORT_C, &port_c) != 0)
    {
        io->digital_inputs = 0u;
        return;
    }

    io->digital_inputs = (uint16_t)port_c;
}

int
leap_board_port_io_ready(void)
{
    return g_mcc.io_ready;
}

const char*
leap_board_pci_address(void)
{
    return (g_mcc.io_ready != 0) ? g_mcc.pci_address : NULL;
}
