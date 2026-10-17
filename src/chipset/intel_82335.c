/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of the Intel 82335(KU82335) chipset.
 *
 * Authors: Tiseno100
 *
 *          Copyright 2021 Tiseno100.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/timer.h>
#include <86box/io.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/chipset.h>
#include <86box/plat_unused.h>

/* Shadow capabilities */
#define DISABLED_SHADOW (MEM_READ_EXTANY | MEM_WRITE_EXTANY)
#define ENABLED_SHADOW  ((LOCK_STATUS) ? RO_SHADOW : RW_SHADOW)
#define RW_SHADOW       (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL)
#define RO_SHADOW       (MEM_READ_INTERNAL | MEM_WRITE_DISABLED)

/* Granularity Register Enable & Recalc */
#define EXTENDED_GRANULARITY_ENABLED (dev->regs[0x2c] & 0x01)
#define GRANULARITY_RECALC           ((dev->regs[0x2e] & (1 << (i + 8))) ? ((dev->regs[0x2e] & (1 << i)) ? RO_SHADOW : RW_SHADOW) : DISABLED_SHADOW)

/* R/W operator for the Video RAM region */
#define DETERMINE_VIDEO_RAM_WRITE_ACCESS ((dev->regs[0x22] & (0x08 << 8)) ? RW_SHADOW : RO_SHADOW)

/* Base System 512/640KB switch */
#define ENABLE_TOP_128KB  (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL)
#define DISABLE_TOP_128KB (MEM_READ_EXTANY | MEM_WRITE_EXTANY)

/* ROM size determination */
#define ROM_SIZE ((dev->regs[0x22] & (0x01 << 8)) ? 0xe0000 : 0xf0000)

/* Lock status */
#define LOCK_STATUS (dev->regs[0x22] & (0x80 << 8))

/* Define Memory Remap Sizes */
#define DEFINE_RC1_REMAP_SIZE ((dev->regs[0x24] & 0x02) ? 128 : 256)
#define DEFINE_RC2_REMAP_SIZE ((dev->regs[0x26] & 0x02) ? 128 : 256)

typedef struct intel_82335_t {
    uint16_t regs[256];

    uint16_t cfg_locked;

    /* BluMach: decode the CC0/CC1 address range compare registers. */
    int      cc_decode;
    int      cc_programmed;
} intel_82335_t;

#ifdef ENABLE_INTEL_82335_LOG
int intel_82335_do_log = ENABLE_INTEL_82335_LOG;

static void
intel_82335_log(const char *fmt, ...)
{
    va_list ap;

    if (intel_82335_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define intel_82335_log(fmt, ...)
#endif

/*
 * BluMach: address range compare registers CC0 (28h) and CC1 (2Ah).
 *
 * 82335 SX data sheet, section 2.3.4.3: bits 15-11 hold the compare data
 * (A23-A19), bits 7-3 the address mask (M23-M19) and bit 0 the enable.  An
 * access inside either enabled range is a local (on-board DRAM) cycle; any
 * other access goes to the bus.  BIOS memory autoscan routines, such as the
 * Olivetti M300 "RAM-REMAPPING" sequence, narrow these ranges and measure
 * how much extended memory answers before choosing the roll compare
 * settings.  Only on-board DRAM above 1 MB is gated, up to the board limit;
 * memory beyond it belongs to bus memory boards and always answers.  The
 * gate is applied once the BIOS first writes CC0 or CC1, so the power-on
 * map stays fully populated.
 */
#define CC_ONBOARD_LIMIT 0x00400000

static int
intel_82335_cc_match(uint16_t reg, uint32_t addr)
{
    uint32_t mask = (reg >> 3) & 0x1f;
    uint32_t cmp  = (reg >> 11) & 0x1f;

    if (!(reg & 0x0001))
        return 0;

    return (((addr >> 19) & mask) == (cmp & mask));
}

static void
intel_82335_cc_recalc(intel_82335_t *dev)
{
    uint32_t top = (uint32_t) mem_size << 10;
    uint32_t addr;
    uint32_t size;
    int      local;

    if (!dev->cc_decode || !dev->cc_programmed)
        return;

    if (top > CC_ONBOARD_LIMIT)
        top = CC_ONBOARD_LIMIT;

    for (addr = 0x00100000; addr < top; addr += 0x00080000) {
        size  = ((top - addr) < 0x00080000) ? (top - addr) : 0x00080000;
        local = intel_82335_cc_match(dev->regs[0x28], addr) ||
                intel_82335_cc_match(dev->regs[0x2a], addr);
        mem_set_mem_state_both(addr, size, local ? (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL) :
                                                   (MEM_READ_EXTANY | MEM_WRITE_EXTANY));
    }

    flushmmucache_nopc();
}

static void
intel_82335_write(uint16_t addr, uint16_t val, void *priv)
{
    intel_82335_t *dev       = (intel_82335_t *) priv;
    uint32_t       romsize   = 0;
    uint32_t       base      = 0;
    uint32_t       rc1_remap = 0;
    uint32_t       rc2_remap = 0;

    dev->regs[addr] = val;

    if (!dev->cfg_locked) {

        intel_82335_log("Register %02x: Write %04x\n", addr, val);

        switch (addr) {
            case 0x22: /* Memory Controller */

                /* BluMach: S640 (bit 3) selects 512 KB or 640 KB of local
                   base memory.  With 512 KB, 80000h-9FFFFh is a bus cycle;
                   the Olivetti M300 BIOS probes it there to look for a
                   memory board before sizing on-board DRAM. */
                if (dev->cc_decode)
                    mem_set_mem_state_both(0x80000, 0x20000,
                                           (dev->regs[0x22] & 0x08) ? (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL) :
                                                                      (MEM_READ_EXTANY | MEM_WRITE_EXTANY));

                /* Check if the ROM chips are 256 or 512Kbit (Just for Shadowing sanity) */
                romsize = ROM_SIZE;

                if (!EXTENDED_GRANULARITY_ENABLED) {
                    shadowbios       = !!(dev->regs[0x22] & 0x01);
                    shadowbios_write = !!(dev->regs[0x22] & 0x01);

                    /* Base System 512/640KB set */
#if 0
                    mem_set_mem_state_both(0x80000, 0x20000, (dev->regs[0x22] & 0x08) ? ENABLE_TOP_128KB : DISABLE_TOP_128KB);
#endif

                    /* Video RAM shadow*/
                    mem_set_mem_state_both(0xa0000, 0x20000, (dev->regs[0x22] & (0x04 << 8)) ? DETERMINE_VIDEO_RAM_WRITE_ACCESS : DISABLED_SHADOW);

                    /* Option ROM shadow */
                    mem_set_mem_state_both(0xc0000, 0x20000, (dev->regs[0x22] & (0x02 << 8)) ? ENABLED_SHADOW : DISABLED_SHADOW);

                    /* System ROM shadow */
                    mem_set_mem_state_both(0xe0000, 0x20000, (dev->regs[0x22] & 0x01) ? ENABLED_SHADOW : DISABLED_SHADOW);
                }
                break;

            case 0x24: /* Roll Compare (Just top remapping. Not followed according to datasheet!) */
            case 0x26:
                rc1_remap = (dev->regs[0x24] & 0x01) ? DEFINE_RC1_REMAP_SIZE : 0;
                rc2_remap = (dev->regs[0x26] & 0x01) ? DEFINE_RC2_REMAP_SIZE : 0;
                mem_remap_top(rc1_remap + rc2_remap);
                break;

            case 0x28: /* Address Range Compare CC0 */
            case 0x2a: /* Address Range Compare CC1 */
                dev->cc_programmed = 1;
                intel_82335_cc_recalc(dev);
                break;

            case 0x2e: /* Extended Granularity (Enabled if Bit 0 in Register 2Ch is set) */
                if (EXTENDED_GRANULARITY_ENABLED) {
                    for (uint8_t i = 0; i < 8; i++) {
                        base             = 0xc0000 + (i << 15);
                        shadowbios       = (dev->regs[0x2e] & (1 << (i + 8))) && (base == romsize);
                        shadowbios_write = (dev->regs[0x2e] & (1 << i)) && (base == romsize);
                        mem_set_mem_state_both(base, 0x8000, GRANULARITY_RECALC);
                    }
                }
                break;

            default:
                break;
        }
    }

    /* Unlock/Lock configuration registers */
    dev->cfg_locked = LOCK_STATUS;
}

static uint16_t
intel_82335_read(uint16_t addr, void *priv)
{
    const intel_82335_t *dev = (intel_82335_t *) priv;

    intel_82335_log("Register %02x: Read %04x\n", addr, dev->regs[addr]);

    return dev->regs[addr];
}

static void
intel_82335_close(void *priv)
{
    intel_82335_t *dev = (intel_82335_t *) priv;

    free(dev);
}

static void *
intel_82335_init(const device_t *info)
{
    intel_82335_t *dev = (intel_82335_t *) calloc(1, sizeof(intel_82335_t));

    memset(dev->regs, 0, sizeof(dev->regs));

    dev->cc_decode = !!info->local;

    dev->regs[0x22] = 0x08;
    dev->regs[0x28] = 0xf9;

    dev->cfg_locked = 0;

    /* Memory Configuration */
    io_sethandler(0x0022, 0x0001, NULL, intel_82335_read, NULL, NULL, intel_82335_write, NULL, dev);

    /* Roll Comparison */
    io_sethandler(0x0024, 0x0001, NULL, intel_82335_read, NULL, NULL, intel_82335_write, NULL, dev);
    io_sethandler(0x0026, 0x0001, NULL, intel_82335_read, NULL, NULL, intel_82335_write, NULL, dev);

    /* Address Range Comparison */
    io_sethandler(0x0028, 0x0001, NULL, intel_82335_read, NULL, NULL, intel_82335_write, NULL, dev);
    io_sethandler(0x002a, 0x0001, NULL, intel_82335_read, NULL, NULL, intel_82335_write, NULL, dev);

    /* Granularity Enable */
    io_sethandler(0x002c, 0x0001, NULL, intel_82335_read, NULL, NULL, intel_82335_write, NULL, dev);

    /* Extended Granularity */
    io_sethandler(0x002e, 0x0001, NULL, intel_82335_read, NULL, NULL, intel_82335_write, NULL, dev);

    return dev;
}

const device_t intel_82335_device = {
    .name          = "Intel 82335",
    .internal_name = "intel_82335",
    .flags         = 0,
    .local         = 0,
    .init          = intel_82335_init,
    .close         = intel_82335_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

/* BluMach: variant that decodes the CC0/CC1 local memory ranges. */
const device_t intel_82335_cc_device = {
    .name          = "Intel 82335 (address range decode)",
    .internal_name = "intel_82335_cc",
    .flags         = 0,
    .local         = 1,
    .init          = intel_82335_init,
    .close         = intel_82335_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
