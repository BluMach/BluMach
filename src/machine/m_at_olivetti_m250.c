/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 * BluMach preservation port: rtzor, Project BluMach, 2026.
 *
 *          Implementation of the Olivetti M250 / M250 E desktop.
 *
 *          The M250 (1989) and M250 E (1989) are Olivetti's first
 *          AT-class desktop machines with an 80286 CPU. The M250
 *          runs at 8 MHz, the M250 E at 12 MHz. The peripheral
 *          controller is the CHIPS 80C206 (M250) or 82C206 (M250 E)
 *          — a standard AT peripheral controller (8259 PIC + 8237
 *          DMA + 8254 PIT + RTC). Because it is a stock CHIPS chip
 *          and not a custom gate array, it does NOT need its own
 *          chipset driver — machine_at_common_ide_init() adds the
 *          standard AT peripheral set (PIC, DMA, PIT, RTC, ISA IDE)
 *          automatically.
 *
 *          The CUSTOM Olivetti silicon is the memory controller:
 *            M250   = GA98 + GA99
 *            M250 E = GA80 + GA99
 *          These are the chips that decode ports 0x65 / 0x67 / 0x69
 *          (and 0x6B on the E variant). See olivetti_m250_gate.c.
 *
 *          Phoenix v1.42 quirks are identical to the Olivetti
 *          PCS 286 (see m_at_olivetti_286.c):
 *            - RAM-REMAPPING test 6 needs 0x60000 aliased to 0x80000
 *            - BDA 0x410 must be 0x20 (80x25 color, 1 FDD) at boot
 *            - NVR factory defaults must match (handled in nvr_at.c)
 *
 * Authors: rtzor
 *
 *          Copyright 2026 rtzor.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/plat_unused.h>
#include "cpu.h"
#include <86box/timer.h>
#include <86box/io.h>
#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/keyboard.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/fdc_ext.h>
#include <86box/hdc.h>
#include <86box/nvr.h>
#include <86box/sio.h>
#include <86box/serial.h>
#include <86box/video.h>
#include <86box/machine.h>

/* Shared body for both M250 (8 MHz) and M250 E (12 MHz). They use
   different system BIOS images and gate-array revisions, while the
   KBC, FDC and PERC/PDSD 1.06 OVC video ROM are shared.
   The gate array device's "E variant" flag is set by the M250 E
   init after the shared work is done (so port 0x6B is registered
   before the first POST access). */
static int
machine_at_olivetti_m250_init_internal(const machine_t *model, int is_e_variant)
{
    int ret;

    /* Main system BIOS: 64KB flat, loaded at 0xF0000. The stock
       M250 uses PERB 1.03; the M250 E uses its distinct PDSF 1.06.
       Each image is a single binary (not byte-interleaved) — the
       cap6_m250.pdf service guide shows two 27C256 sockets on the
       M250 motherboard but the preserved dumps ship them merged. */
    const char *bios_path = is_e_variant
                              ? "roms/machines/olivetti_m250/olivetti_m250e_pdsf_106.BIN"
                              : "roms/machines/olivetti_m250/olivetti_m250_perb_103.BIN";
    ret = bios_load_linear(bios_path,
                           0x000f0000, 65536, 0);

    if (bios_only || !ret)
        return ret;

    /* Standard AT core plus the motherboard's ISA IDE interface.
       Adds PIC1, PIC2, DMA controller (8-bit and 16-bit), PIT,
       RTC, the standard port_6x_device (A20 gate, M250 has stock
       Intel 8742 KBC so the standard port_6x works — no need for
       port_6x_olivetti like the PCS 286), game port, and the IDE
       ISA device. The CHIPS 80C206/82C206 peripheral controller
       does not need a separate driver — its functions are
       supplied by the standard AT peripheral set. */
    machine_at_common_ide_init(model);

    /* Top 384K of the 1MB address space is mapped to RAM. Mirrors
       the PCS 286 layout and matches the cap6_m250.pdf memory map
       (640K base + 384K ext). The Phoenix v1.42 POST expects this
       layout: it copies the upper 128K of BIOS (E0000-FFFFF) to
       shadow RAM at boot, then enables shadowing via port 0x69
       bit 3 on the gate array. */
    /* With 1 MiB the gate array owns the 384 KiB behind the hole: port 69h
       bits 4/7 remap it to 100000h and bit 3 shadows E0000h-FFFFFh (see
       olivetti_m250_gate.c).  Other sizes keep the linear remap. */
    if (mem_size != 1024)
        mem_remap_top(384);

    /* Phoenix v1.42 diagnostic compare expects BDA equip word 0x410
       to use the same encoding as NVR 0x14. The IBM AT encoding is:
         bit 5 = 80x25 color (1), bit 4 = 40x25 color (0)
         bit 0 = IPL (boot from FDD)
         bit 6 = 2 FDDs (0 = 1 FDD)
         bit 1 = FPU installed
       86Box's generic BDA init writes 0x10 (bit 4 set, which is
       40x25 color) without the IPL bit. The Olivetti has 80x25
       color and 1 FDD, so the correct BDA value is 0x20 (bit 5 =
       80x25 color). Without this override Phoenix reads BDA 0x10,
       compares against NVR 0x20, finds a "mismatch", and sets bit
       5 of CMOS 0x0E (Equipment Error), which keeps the SET-UP
       showing on every boot.

       This is a ONE-SHOT write at machine init (not a per-poll
       force — that was a hack and is now removed for the M250).
       86Box's BDA init runs during mem.c setup before this
       machine init, so we overwrite the generic value with the
       Olivetti-specific one. Phoenix will then read 0x20, find it
       matches NVR 0x14, and skip the equipment-error flag. */
    extern uint8_t *ram;
    if (ram != NULL) {
        /* Match NVR 0x14 (0x21) bit-for-bit so Phoenix v1.42's BDA-vs-NVR
         * diagnostic compare does not flag 0x0E bit 6 ("Configuration
         * table integrity"). Bit 0 = IPL (boot from FDD), bit 5 = 80x25
         * color, bit 6 = 2 FDDs (0 = 1 FDD). 0x21 = IPL + 80x25 + 1 FDD. */
        ram[0x0410] = 0x21;
        ram[0x0411] = 0x00;
    }

    /* The Olivetti-specific NVR factory defaults (IPL + 80x25 + 1
       FDD, 640K base, 384K ext, 0x0E clean) are set inside
       nvr_at.c when it detects this machine, following the same
       is_new pattern as the PCS 286 block at nvr_at.c:1228.
       Together with the BDA override above, this makes Phoenix
       v1.42's diagnostic compare pass and skip the SET-UP on
       subsequent boots. */
    /* Olivetti M250 gate array (GA98/GA99 on the stock M250, GA80
       on the M250 E). Handles ports 0x65/0x67/0x69/0x6B. NOT the
       same as the IOC02 (PCS 286 / PCS 386SX) — the M250 does not
       have an IOC02 chip. */
    device_add(&olivetti_m250_gate_device);
    if (is_e_variant) {
        /* M250 E has the GA80 + GA99 silicon which adds port 0x6B
           (memory banks starting address). The setter must be
           called AFTER device_add() so the static handle is
           populated, and BEFORE the first POST access to 0x6B. */
        extern void olivetti_m250_gate_set_e_variant(int on);
        olivetti_m250_gate_set_e_variant(1);
    }

    /* AT-style PS/2 KBC. The cap6 service manual lists the chip as
       a "stock 8742", but the Phoenix v1.42 firmware expects the
       Olivetti-customized command set (0x80/0x82/0x84/0x85/0xCF) and
       loops forever rejecting them as "bad controller command" if
       we use kbc_params=0 (KBC_VEN_NONE). Use KBC_VEN_OLIVETTI — the
       same vendor the PCS 286 uses — so the write_cmd_olivetti
       handler in kbc_at.c is installed and Phoenix POST completes. */
    device_add_params(machine_get_kbc_device(machine), (void *) model->kbc_params);

    /* WD37C65C FDC is the floppy controller on the M250 motherboard.
       The cap6 service manual lists it explicitly (BA227/BA233 boards
       for the stock M250, BA241 for the M250 E). */
    device_add(&fdc_at_device);

    /* No low-memory alias.  PERB 1.03 uses "RAM-REMAPPING" only as a
       sizing probe: F000:0361 writes it at 0000:0000 and expects 8000:0000
       to differ (no wrap), and F000:68FC compares 3FFF:0000 with the
       protected-mode window at 19FFF0h.  A permanent 60000h->80000h alias
       is not needed and made 128 KiB of conventional memory overlap. */

    /* On-board Paradise PVGA1A with 256 KB.  Olivetti calls it OVC; its
       1.06 video BIOS (PERC on the M250, PDSD on the M250 E, identical
       bytes) sits in its own ROM at C0000h and is loaded by the device. */
    if (gfxcard[0] == VID_INTERNAL)
        device_add(&paradise_pvga1a_m250_device);

    return ret;
}

int
machine_at_olivetti_m250_init(const machine_t *model)
{
    /* Stock M250: 8 MHz, GA98 + GA99, no port 0x6B. */
    return machine_at_olivetti_m250_init_internal(model, 0);
}

int
machine_at_olivetti_m250e_init(const machine_t *model)
{
    /* M250 E: 12 MHz, PDSF 1.06 BIOS, GA80 + GA99, port 0x6B
       enabled. The KBC, FDC and video ROM are shared. The clock
       difference is handled by the machine_table.c entry (cpu block) — this
       init function only needs to enable the E variant of the
       gate array. */
    return machine_at_olivetti_m250_init_internal(model, 1);
}
