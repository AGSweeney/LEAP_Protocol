/*
 * mcc_dio24_cli.c — Command-line read/write for MCC PCI-DIO-24 / DIO-24H.
 *
 * Requires root (ioperm). Stop leap-device before using to avoid port contention.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "mcc_pci_dio24h.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MCC_DIO24_SHADOW_PATH "/tmp/mcc-dio24.shadow"
#define MCC_DIO24_SHADOW_MAGIC 0x4d434332u

static uint16_t g_io_base_override;
static int      g_shadow_dirty;

static const char* port_label(MccPciDio24hPort port);

static void
load_output_shadow(MccPciDio24h* dev)
{
    FILE*         fp;
    unsigned long magic;
    size_t        i;

    fp = fopen(MCC_DIO24_SHADOW_PATH, "rb");
    if (fp == NULL)
    {
        return;
    }

    if (fread(&magic, sizeof(magic), 1u, fp) != 1u || magic != MCC_DIO24_SHADOW_MAGIC)
    {
        fclose(fp);
        return;
    }

    for (i = 0u; i < 3u; ++i)
    {
        int ch = fgetc(fp);
        if (ch < 0)
        {
            break;
        }
        dev->output_shadow[i] = (uint8_t)ch;
    }

    fclose(fp);
}

static void
save_output_shadow(const MccPciDio24h* dev)
{
    FILE*         fp;
    unsigned long magic = MCC_DIO24_SHADOW_MAGIC;
    size_t        i;

    if (dev == NULL)
    {
        return;
    }

    fp = fopen(MCC_DIO24_SHADOW_PATH, "wb");
    if (fp == NULL)
    {
        return;
    }

    (void)fwrite(&magic, sizeof(magic), 1u, fp);
    for (i = 0u; i < 3u; ++i)
    {
        fputc(dev->output_shadow[i], fp);
    }
    fclose(fp);
}

static int
replay_output_shadow(MccPciDio24h* dev)
{
    if (mcc_pci_dio24h_port_is_output(dev, MCC_PCI_DIO24H_PORT_A))
    {
        if (mcc_pci_dio24h_write_port(
                dev, MCC_PCI_DIO24H_PORT_A, dev->output_shadow[MCC_PCI_DIO24H_PORT_A]) != 0)
        {
            return -1;
        }
    }

    if (mcc_pci_dio24h_port_is_output(dev, MCC_PCI_DIO24H_PORT_B))
    {
        if (mcc_pci_dio24h_write_port(
                dev, MCC_PCI_DIO24H_PORT_B, dev->output_shadow[MCC_PCI_DIO24H_PORT_B]) != 0)
        {
            return -1;
        }
    }

    if (mcc_pci_dio24h_port_is_output(dev, MCC_PCI_DIO24H_PORT_C))
    {
        if (mcc_pci_dio24h_write_port(
                dev, MCC_PCI_DIO24H_PORT_C, dev->output_shadow[MCC_PCI_DIO24H_PORT_C]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static void
print_port_values(MccPciDio24h* dev, MccPciDio24hPort port, const char* prefix)
{
    uint8_t latch = 0u;
    uint8_t raw   = 0u;

    (void)mcc_pci_dio24h_read_port(dev, port, &latch);
    (void)mcc_pci_dio24h_read_port_raw(dev, port, &raw);
    printf("%sport %s: latch 0x%02x raw 0x%02x%s\n",
           prefix,
           port_label(port),
           latch,
           raw,
           mcc_pci_dio24h_port_is_output(dev, port) ? "" : " (input pins)");
}

static void
usage(const char* prog)
{
    fprintf(stderr,
            "Usage: %s [options] <command> [args]\n"
            "\n"
            "Options:\n"
            "  --io-base HEX   PCI I/O base override (or env MCC_DIO24_IO_BASE)\n"
            "\n"
            "Commands:\n"
            "  info            Show PCI address and I/O base\n"
            "  read [a|b|c]    Read port(s); outputs show latch + raw pin levels\n"
            "  peek            Dump 8255 control word and raw/latch values\n"
            "  write PORT VAL  Write port a|b|c (VAL hex/dec)\n"
            "  write24 VAL     Write 24-bit value to ports A,B,C\n"
            "  safe            Drive outputs A/B to zero\n"
            "  config MODE     8255 directions: leap | all-in | all-out\n"
            "\n"
            "  leap profile:    A=out B=out C=in  (matches leap-device MCC board)\n"
            "\n"
            "Examples:\n"
            "  %s info\n"
            "  %s read\n"
            "  %s read c\n"
            "  %s write a 0x55\n"
            "  %s write24 0x0000ff\n"
            "  %s safe\n"
            "\n"
            "Note: stop /etc/init.d/leap-device while testing raw port I/O.\n",
            prog,
            prog,
            prog,
            prog,
            prog,
            prog,
            prog);
}

static int
parse_io_base_arg(const char* text, uint16_t* io_base_out)
{
    char* endp;
    unsigned long value;

    if (text == NULL || io_base_out == NULL)
    {
        return -1;
    }

    errno = 0;
    value = strtoul(text, &endp, 0);
    if (errno != 0 || endp == text || value > 0xfffful)
    {
        return -1;
    }

    *io_base_out = (uint16_t)value;
    return 0;
}

static int
parse_port_name(const char* name, MccPciDio24hPort* port_out)
{
    if (name == NULL || port_out == NULL || name[0] == '\0' || name[1] != '\0')
    {
        return -1;
    }

    switch (name[0])
    {
    case 'a':
    case 'A':
        *port_out = MCC_PCI_DIO24H_PORT_A;
        return 0;
    case 'b':
    case 'B':
        *port_out = MCC_PCI_DIO24H_PORT_B;
        return 0;
    case 'c':
    case 'C':
        *port_out = MCC_PCI_DIO24H_PORT_C;
        return 0;
    default:
        return -1;
    }
}

static const char*
port_label(MccPciDio24hPort port)
{
    switch (port)
    {
    case MCC_PCI_DIO24H_PORT_A:
        return "A";
    case MCC_PCI_DIO24H_PORT_B:
        return "B";
    case MCC_PCI_DIO24H_PORT_C:
        return "C";
    default:
        return "?";
    }
}

static int
apply_config_mode(MccPciDio24h* dev, const char* mode)
{
    if (mode == NULL)
    {
        return -1;
    }

    if (strcmp(mode, "leap") == 0)
    {
        return mcc_pci_dio24h_configure_ports(
            dev,
            MCC_PCI_DIO24H_DIR_OUTPUT,
            MCC_PCI_DIO24H_DIR_OUTPUT,
            MCC_PCI_DIO24H_DIR_INPUT,
            MCC_PCI_DIO24H_DIR_INPUT);
    }

    if (strcmp(mode, "all-in") == 0)
    {
        return mcc_pci_dio24h_configure_all_inputs(dev);
    }

    if (strcmp(mode, "all-out") == 0)
    {
        return mcc_pci_dio24h_configure_all_outputs(dev);
    }

    return -1;
}

static int
open_device(MccPciDio24h* dev, const char* config_mode)
{
    MccPciDio24hConfig config;

    mcc_pci_dio24h_init(dev);
    memset(&config, 0, sizeof(config));
    if (g_io_base_override != 0u)
    {
        config.io_base_override = g_io_base_override;
    }

    if (mcc_pci_dio24h_open(dev, &config) != 0)
    {
        fprintf(stderr, "error: open failed (%s)\n", strerror(dev->last_errno));
        return -1;
    }

    if (config_mode != NULL && apply_config_mode(dev, config_mode) != 0)
    {
        fprintf(stderr, "error: config %s failed (%s)\n", config_mode, strerror(errno));
        return -1;
    }

    if (config_mode != NULL)
    {
        load_output_shadow(dev);
        if (replay_output_shadow(dev) != 0)
        {
            fprintf(stderr, "error: replay saved outputs failed (%s)\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int
cmd_info(MccPciDio24h* dev)
{
    printf("pci=%s io_base=0x%04x ready=%d\n",
           dev->pci_address,
           (unsigned)dev->io_base,
           dev->io_ready);
    return 0;
}

static int
cmd_read(MccPciDio24h* dev, const char* port_name)
{
    if (port_name == NULL)
    {
        uint32_t value24;

        print_port_values(dev, MCC_PCI_DIO24H_PORT_A, "");
        print_port_values(dev, MCC_PCI_DIO24H_PORT_B, "");
        print_port_values(dev, MCC_PCI_DIO24H_PORT_C, "");
        if (mcc_pci_dio24h_read_24(dev, &value24) != 0)
        {
            fprintf(stderr, "error: read24 failed (%s)\n", strerror(errno));
            return 1;
        }
        printf("24-bit latch view: 0x%06x (%u)\n", (unsigned)value24, (unsigned)value24);
        return 0;
    }

    MccPciDio24hPort port;

    if (parse_port_name(port_name, &port) != 0)
    {
        fprintf(stderr, "error: invalid port '%s' (use a, b, or c)\n", port_name);
        return 1;
    }

    print_port_values(dev, port, "");
    return 0;
}

static int
cmd_peek(MccPciDio24h* dev)
{
    printf("pci=%s io_base=0x%04x control=0x%02x\n",
           dev->pci_address,
           (unsigned)dev->io_base,
           (unsigned)dev->control_word);
    print_port_values(dev, MCC_PCI_DIO24H_PORT_A, "");
    print_port_values(dev, MCC_PCI_DIO24H_PORT_B, "");
    print_port_values(dev, MCC_PCI_DIO24H_PORT_C, "");
    return 0;
}

static int
cmd_write_port(MccPciDio24h* dev, const char* port_name, const char* value_text)
{
    MccPciDio24hPort port;
    char*              endp;
    unsigned long      value;

    if (parse_port_name(port_name, &port) != 0)
    {
        fprintf(stderr, "error: invalid port '%s' (use a, b, or c)\n", port_name);
        return 1;
    }

    errno = 0;
    value = strtoul(value_text, &endp, 0);
    if (errno != 0 || endp == value_text || value > 0xfful)
    {
        fprintf(stderr, "error: invalid value '%s'\n", value_text);
        return 1;
    }

    if (mcc_pci_dio24h_write_port(dev, port, (uint8_t)value) != 0)
    {
        fprintf(stderr, "error: write port %s failed (%s)\n",
                port_label(port),
                strerror(errno));
        return 1;
    }

    g_shadow_dirty = 1;
    printf("port %s <= 0x%02x\n", port_label(port), (unsigned)(uint8_t)value);
    print_port_values(dev, port, "  readback ");
    return 0;
}

static int
cmd_write24(MccPciDio24h* dev, const char* value_text)
{
    char*         endp;
    unsigned long value;

    errno = 0;
    value = strtoul(value_text, &endp, 0);
    if (errno != 0 || endp == value_text || value > 0xfffffful)
    {
        fprintf(stderr, "error: invalid 24-bit value '%s'\n", value_text);
        return 1;
    }

    if (mcc_pci_dio24h_write_24(dev, (uint32_t)value) != 0)
    {
        fprintf(stderr, "error: write24 failed (%s)\n", strerror(errno));
        return 1;
    }

    g_shadow_dirty = 1;
    printf("24-bit <= 0x%06x\n", (unsigned)(uint32_t)value);
    print_port_values(dev, MCC_PCI_DIO24H_PORT_A, "  readback ");
    print_port_values(dev, MCC_PCI_DIO24H_PORT_B, "  readback ");
    print_port_values(dev, MCC_PCI_DIO24H_PORT_C, "  readback ");
    return 0;
}

static int
cmd_safe(MccPciDio24h* dev)
{
    if (mcc_pci_dio24h_write_port(dev, MCC_PCI_DIO24H_PORT_A, 0u) != 0 ||
        mcc_pci_dio24h_write_port(dev, MCC_PCI_DIO24H_PORT_B, 0u) != 0)
    {
        fprintf(stderr, "error: safe failed (%s)\n", strerror(errno));
        return 1;
    }

    g_shadow_dirty = 1;
    puts("outputs A/B cleared");
    return 0;
}

static int
cmd_config(MccPciDio24h* dev, const char* mode)
{
    if (mode == NULL || apply_config_mode(dev, mode) != 0)
    {
        fprintf(stderr, "error: config requires leap, all-in, or all-out\n");
        return 1;
    }

    if (strcmp(mode, "leap") == 0)
    {
        puts("config: A=out B=out C=in (leap profile)");
    }
    else if (strcmp(mode, "all-in") == 0)
    {
        puts("config: all inputs");
    }
    else
    {
        puts("config: all outputs");
    }

    return 0;
}

int
main(int argc, char** argv)
{
    MccPciDio24h dev;
    const char*  prog;
    const char*  cmd;
    int          argi;
    int          rc;

    prog = (argc > 0 && argv[0] != NULL) ? argv[0] : "mcc-dio24";
    argi = 1;

    while (argi < argc && argv[argi][0] == '-')
    {
        if (strcmp(argv[argi], "--io-base") == 0)
        {
            if (argi + 1 >= argc ||
                parse_io_base_arg(argv[argi + 1], &g_io_base_override) != 0)
            {
                fprintf(stderr, "error: --io-base requires a hex value\n");
                return 1;
            }
            argi += 2;
            continue;
        }

        if (strcmp(argv[argi], "-h") == 0 || strcmp(argv[argi], "--help") == 0)
        {
            usage(prog);
            return 0;
        }

        fprintf(stderr, "error: unknown option '%s'\n", argv[argi]);
        usage(prog);
        return 1;
    }

    if (g_io_base_override == 0u)
    {
        const char* env = getenv("MCC_DIO24_IO_BASE");
        if (env != NULL && env[0] != '\0')
        {
            (void)parse_io_base_arg(env, &g_io_base_override);
        }
    }

    if (argi >= argc)
    {
        usage(prog);
        return 1;
    }

    cmd = argv[argi++];

    if (strcmp(cmd, "info") == 0)
    {
        if (open_device(&dev, NULL) != 0)
        {
            return 1;
        }
        rc = cmd_info(&dev);
    }
    else if (strcmp(cmd, "config") == 0)
    {
        if (argi + 1 > argc)
        {
            fprintf(stderr, "error: config requires MODE\n");
            return 1;
        }
        if (open_device(&dev, NULL) != 0)
        {
            return 1;
        }
        rc = cmd_config(&dev, argv[argi]);
    }
    else
    {
        if (open_device(&dev, "leap") != 0)
        {
            return 1;
        }

        if (strcmp(cmd, "read") == 0)
        {
            const char* port = (argi < argc) ? argv[argi] : NULL;
            if (port != NULL && argi + 1 < argc)
            {
                fprintf(stderr, "error: read takes at most one port argument\n");
                rc = 1;
            }
            else
            {
                rc = cmd_read(&dev, port);
            }
        }
        else if (strcmp(cmd, "write") == 0)
        {
            if (argi + 2 > argc)
            {
                fprintf(stderr, "error: write requires PORT and VALUE\n");
                rc = 1;
            }
            else
            {
                rc = cmd_write_port(&dev, argv[argi], argv[argi + 1]);
            }
        }
        else if (strcmp(cmd, "write24") == 0)
        {
            if (argi + 1 > argc)
            {
                fprintf(stderr, "error: write24 requires VALUE\n");
                rc = 1;
            }
            else
            {
                rc = cmd_write24(&dev, argv[argi]);
            }
        }
        else if (strcmp(cmd, "safe") == 0)
        {
            if (argi < argc)
            {
                fprintf(stderr, "error: safe takes no arguments\n");
                rc = 1;
            }
            else
            {
                rc = cmd_safe(&dev);
            }
        }
        else if (strcmp(cmd, "peek") == 0)
        {
            if (argi < argc)
            {
                fprintf(stderr, "error: peek takes no arguments\n");
                rc = 1;
            }
            else
            {
                rc = cmd_peek(&dev);
            }
        }
        else
        {
            fprintf(stderr, "error: unknown command '%s'\n", cmd);
            usage(prog);
            rc = 1;
        }
    }

    if (strcmp(cmd, "info") != 0 && strcmp(cmd, "config") != 0)
    {
        save_output_shadow(&dev);
    }

    mcc_pci_dio24h_close(&dev);
    return rc;
}
