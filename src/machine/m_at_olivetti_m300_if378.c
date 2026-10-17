/*
 * BluMach, a preservation-focused fork of 86Box.
 *
 *          Olivetti M300 modular system with IF378 80386SX CPU card.
 *
 * Author: rtzor
 * Project: BluMach
 *
 * Copyright 2026 rtzor.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/timer.h>
#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/io.h>
#include <86box/keyboard.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/fdc_ext.h>
#include <86box/serial.h>
#include <86box/machine.h>

static uint8_t  m300_if378_last_diag;
static uint16_t m300_if378_mcr;
static uint8_t  m300_if378_low_c00[0x100];

/*
 * DRAM row/column scrambling seen by the "RAM-REMAPPING" sizing code.
 *
 * During diagnostic step 07 the BIOS (F000:0326) switches the 82335 MCR from
 * 256 Kbit to 1 Mbit DRAM (DSIZE, bit 4) and then to two interleaved banks
 * (INTERL, bit 6).  On the real board the change of address multiplexing
 * moves data already in DRAM: the BIOS copies 0C00h-0FFFh back to
 * 0000h-03FFh after DSIZE (F000:034D) and 1400h-14FFh back to 0C00h after
 * INTERL (F000:039A).  BluMach has a linear memory array, so those copies
 * would replace the interrupt vectors with unrelated data.  Reproduce only
 * the two moves the BIOS undoes, and only in step 07; the earlier memory
 * test toggles the same bits and must see unmodified memory.
 */
static void
m300_if378_mcr_write(uint16_t port, uint16_t val, void *priv)
{
    uint16_t old = m300_if378_mcr;

    (void) port;
    (void) priv;

    m300_if378_mcr = val;

    if ((m300_if378_last_diag != 0x07) || (ram == NULL))
        return;

    if (!(old & 0x0010) && (val & 0x0010)) {
        memcpy(m300_if378_low_c00, &ram[0x0c00], sizeof(m300_if378_low_c00));
        memcpy(&ram[0x0c00], &ram[0x0000], 0x0400);
    }

    if ((val & 0x0010) && !(old & 0x0040) && (val & 0x0040))
        memcpy(&ram[0x1400], m300_if378_low_c00, sizeof(m300_if378_low_c00));
}

static void
m300_if378_diag_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) priv;

    m300_if378_last_diag = val;
}

static uint8_t
m300_if378_pit_wait_read(uint16_t port, void *priv)
{
    (void) port;
    (void) priv;
    cycles -= 5;
    return 0xff;
}

static void
m300_if378_pit_wait_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) val;
    (void) priv;
    cycles -= 5;
}

static uint8_t
m300_if378_port61_wait_read(uint16_t port, void *priv)
{
    (void) port;
    (void) priv;
    cycles -= 5;
    return 0xff;
}

static uint8_t
m300_if378_cmos_wait_read(uint16_t port, void *priv)
{
    (void) port;
    (void) priv;
    cycles -= 160;
    return 0xff;
}

static void
m300_if378_cmos_wait_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) val;
    (void) priv;
    cycles -= 160;
}

int
machine_at_olivetti_m300_if378_init(const machine_t *model)
{
    int ret;

    ret = bios_load_linear("roms/machines/m300_if378/BIOS.ROM",
                           0x000f0000, 65536, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_ide_init(model);

    device_add(&intel_82335_cc_device);
    m300_if378_last_diag = 0x00;
    m300_if378_mcr       = 0x0000;
    io_sethandler(0x0022, 1, NULL, NULL, NULL,
                  NULL, m300_if378_mcr_write, NULL, NULL);
    device_add_params(machine_get_kbc_device(machine),
                      (void *) model->kbc_params);

    if (fdc_current[0] == FDC_INTERNAL)
        device_add(&fdc_at_device);
    device_add_inst(&ns16450_device, 1);

    io_sethandler(0x0378, 1, NULL, NULL, NULL,
                  m300_if378_diag_write, NULL, NULL, NULL);
    io_sethandler(0x0040, 4,
                  m300_if378_pit_wait_read, NULL, NULL,
                  m300_if378_pit_wait_write, NULL, NULL, NULL);
    io_sethandler(0x0061, 1,
                  m300_if378_port61_wait_read, NULL, NULL,
                  NULL, NULL, NULL, NULL);
    io_sethandler(0x0070, 2,
                  m300_if378_cmos_wait_read, NULL, NULL,
                  m300_if378_cmos_wait_write, NULL, NULL, NULL);
    return ret;
}
