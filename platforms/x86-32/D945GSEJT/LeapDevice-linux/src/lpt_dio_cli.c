/*
 * lpt_dio_cli.c — Command-line read/write for D945GSEJT LPT1 @ 0x378.
 *
 * Requires root (iopl/ioperm). Stop leap-device before raw port testing.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_board.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LPT_DIO_SHADOW_PATH "/tmp/lpt-dio.shadow"
#define LPT_DIO_SHADOW_MAGIC 0x4c505431u /* "LPT1" */

static LeapRtemsBoardIo g_io;

static int
load_output_shadow(void)
{
    FILE*         fp;
    unsigned long magic;
    int           ch;

    fp = fopen(LPT_DIO_SHADOW_PATH, "rb");
    if (fp == NULL)
    {
        return 0;
    }

    if (fread(&magic, sizeof(magic), 1u, fp) != 1u || magic != LPT_DIO_SHADOW_MAGIC)
    {
        fclose(fp);
        return 0;
    }

    ch = fgetc(fp);
    fclose(fp);
    if (ch < 0)
    {
        return 0;
    }

    g_io.digital_outputs = (uint16_t)(uint8_t)ch;
    return 1;
}

static void
save_output_shadow(void)
{
    FILE*         fp;
    unsigned long magic = LPT_DIO_SHADOW_MAGIC;
    uint8_t       out8  = (uint8_t)(g_io.digital_outputs & 0xffu);

    fp = fopen(LPT_DIO_SHADOW_PATH, "wb");
    if (fp == NULL)
    {
        return;
    }

    (void)fwrite(&magic, sizeof(magic), 1u, fp);
    fputc(out8, fp);
    fclose(fp);
}

static void
usage(const char* prog)
{
    fprintf(stderr,
            "Usage: %s <command> [args]\n"
            "\n"
            "Commands:\n"
            "  info            Show LPT base, ready state, and register addresses\n"
            "  read            Read LEAP-mapped 8x8 inputs and current outputs\n"
            "  read-phys       Read 5 physical status inputs (no D0..D2 mirror)\n"
            "  write VAL       Write outputs D0..D7 using LEAP logic (1=on, active-low port)\n"
            "  safe            Clear LEAP outputs D0..D7 (all off, data port 0xFF)\n"
            "  peek            Dump raw data/status/control + LEAP mapping\n"
            "\n"
            "LEAP inputs (read / peek):\n"
            "  in0..4  status pins ACK#, BUSY, PERROR, SELECT, FAULT#\n"
            "  in5..7  mirror of outputs D0..D2 (same as leap-device 8x8 profile)\n"
            "\n"
            "Examples:\n"
            "  %s info\n"
            "  %s write 0x55\n"
            "  %s read\n"
            "  %s peek\n"
            "\n"
            "Note: stop /etc/init.d/leap-device while testing raw LPT I/O.\n",
            prog,
            prog,
            prog,
            prog,
            prog);
}

static int
open_board(void)
{
    leap_rtems_board_init(&g_io);
    if (leap_board_port_io_ready() == 0)
    {
        fprintf(stderr, "error: LPT port I/O unavailable (need root / iopl)\n");
        return -1;
    }

    if (load_output_shadow() != 0)
    {
        leap_rtems_board_apply_outputs(&g_io, g_io.digital_outputs);
    }

    return 0;
}

static int
cmd_info(void)
{
    printf("board=%s base=0x%04x ready=%d\n",
           leap_board_description(),
           (unsigned)LEAP_LPT1_BASE_ADDR,
           leap_board_port_io_ready());
    printf("data=0x%03x status=0x%03x control=0x%03x\n",
           (unsigned)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_DATA_OFFSET),
           (unsigned)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_STATUS_OFFSET),
           (unsigned)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_CONTROL_OFFSET));
    return 0;
}

static void
print_leap_bits(uint16_t value, const char* label)
{
    unsigned bit;

    printf("%s=0x%02x (", label, (unsigned)(value & 0xffu));
    for (bit = 0u; bit < 8u; ++bit)
    {
        if (bit != 0u)
        {
            putchar(' ');
        }
        printf("D%u=%u", bit, (unsigned)((value >> bit) & 0x1u));
    }
    puts(")");
}

static int
cmd_read(int physical_only)
{
    uint16_t phys;
    uint8_t  data;
    uint8_t  status;
    uint8_t  control;

    leap_board_read_registers(&data, &status, &control);
    phys = leap_board_read_physical_inputs();

    if (physical_only != 0)
    {
        print_leap_bits(phys, "phys inputs");
        printf("raw status=0x%02x\n", (unsigned)status);
        return 0;
    }

    leap_rtems_board_sample_inputs(&g_io);
    print_leap_bits(g_io.digital_outputs, "outputs (logical)");
    print_leap_bits(g_io.digital_inputs, "leap inputs");
    print_leap_bits(phys, "phys inputs");
    printf("raw data=0x%02x (hw port) logical=0x%02x status=0x%02x control=0x%02x\n",
           (unsigned)data,
           (unsigned)LEAP_LPT_HW_TO_LOGICAL(data),
           (unsigned)status,
           (unsigned)control);
    return 0;
}

static int
cmd_write(const char* value_text)
{
    char*         endp;
    unsigned long value;

    errno = 0;
    value = strtoul(value_text, &endp, 0);
    if (errno != 0 || endp == value_text || value > 0xfful)
    {
        fprintf(stderr, "error: invalid value '%s'\n", value_text);
        return 1;
    }

    leap_rtems_board_apply_outputs(&g_io, (uint16_t)value);
    save_output_shadow();

    printf("outputs <= 0x%02x\n", (unsigned)(uint8_t)value);
    return cmd_read(0);
}

static int
cmd_safe(void)
{
    leap_rtems_board_enter_safe(&g_io);
    save_output_shadow();
    puts("outputs cleared");
    return cmd_read(0);
}

static void
print_control_pins(uint8_t control)
{
    printf("raw control=0x%02x pin1(strobe)=%u pin14(autofd)=%u pin16(init)=%u pin17(select)=%u\n",
           (unsigned)control,
           (unsigned)((control >> LEAP_LPT_CONTROL_STROBE_BIT) & 0x1u),
           (unsigned)((control >> LEAP_LPT_CONTROL_AUTOFD_BIT) & 0x1u),
           (unsigned)((control >> LEAP_LPT_CONTROL_INIT_BIT) & 0x1u),
           (unsigned)((control >> LEAP_LPT_CONTROL_SELECT_BIT) & 0x1u));
    puts("control bit=1 => DB25 pin high (relay off on typical CNC BOBs); not LEAP D0..D7");
}

static int
cmd_peek(void)
{
    uint8_t data;
    uint8_t status;
    uint8_t control;

    leap_rtems_board_sample_inputs(&g_io);
    leap_board_read_registers(&data, &status, &control);

    printf("board=%s base=0x%04x ready=%d\n",
           leap_board_description(),
           (unsigned)LEAP_LPT1_BASE_ADDR,
           leap_board_port_io_ready());
    print_leap_bits(g_io.digital_outputs, "outputs (logical)");
    print_leap_bits(g_io.digital_inputs, "leap inputs");
    print_leap_bits(leap_board_read_physical_inputs(), "phys inputs");
    printf("raw data=0x%02x (hw port) logical=0x%02x\n",
           (unsigned)data,
           (unsigned)LEAP_LPT_HW_TO_LOGICAL(data));
    printf("raw status=0x%02x\n", (unsigned)status);
    print_control_pins(control);
    puts("LPT data port is active-low (logical 1 => hw bit 0)");
    puts("status bits: ACK#=6 BUSY=7(inv) PERROR=5 SELECT=4 FAULT#=3");
    return 0;
}

int
main(int argc, char** argv)
{
    const char* prog;
    const char* cmd;
    int         rc = 1;

    prog = (argc > 0 && argv[0] != NULL) ? argv[0] : "lpt-dio";

    if (argc < 2 ||
        strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--help") == 0)
    {
        usage(prog);
        return (argc < 2) ? 1 : 0;
    }

    cmd = argv[1];
    if (open_board() != 0)
    {
        return 1;
    }

    if (strcmp(cmd, "info") == 0)
    {
        rc = cmd_info();
    }
    else if (strcmp(cmd, "read") == 0)
    {
        rc = cmd_read(0);
    }
    else if (strcmp(cmd, "read-phys") == 0)
    {
        rc = cmd_read(1);
    }
    else if (strcmp(cmd, "write") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "error: write requires VALUE\n");
            usage(prog);
            rc = 1;
        }
        else
        {
            rc = cmd_write(argv[2]);
        }
    }
    else if (strcmp(cmd, "safe") == 0)
    {
        rc = cmd_safe();
    }
    else if (strcmp(cmd, "peek") == 0)
    {
        rc = cmd_peek();
    }
    else
    {
        fprintf(stderr, "error: unknown command '%s'\n", cmd);
        usage(prog);
        rc = 1;
    }

    return rc;
}
