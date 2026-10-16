/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          Olivetti M380 / M380 C (BA806 / BA818).
 *
 * BluMach preservation port: rtzor, Project BluMach, 2026.
 *
 *          The first implementation intentionally models only hardware
 *          demonstrated by the service guide and the PBUQ/PBUZ 1.09 ROMs.
 *          The proprietary memory and 32-bit expansion-bus logic is not yet
 *          emulated; standard AT services provide a bring-up baseline.
 */
#include <stdint.h>
#include <stdio.h>
#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/fdc_ext.h>
#include <86box/lpt.h>
#include <86box/machine.h>
#include <86box/rom.h>
#include <86box/serial.h>

/* Dedicated memory (BIOS 1.09).  The 384 KiB behind the A0000h-FFFFFh hole
   is not remapped above the installed RAM: POST tests it at the 32-bit
   address 800A0000h (F000:2051) and reports "Dedicated Memory: 384 KB".
   Its top 128 KiB back the E0000h-FFFFFh shadow window.  Writes to that
   window always reach the shadow RAM; KBC register 2 bit 6 (Olivetti 8742
   extended protocol) selects whether reads come from the RAM (set) or from
   the ROM (clear).  POST copies E000h/F000h onto themselves with the bit
   clear (F000:707C), then sets it to check the copy (F000:70A8) and to run
   from RAM.  Other register 2 bits (bank test bit 7, NVRAM byte 6 in bits
   0-5) are kept by the controller without a modelled effect. */
static mem_mapping_t m380_dedicated_mapping;

static uint8_t
m380_ded_read8(uint32_t addr, void *priv)
{
    (void) priv;
    return ram[0xa0000 + (addr - 0x800a0000)];
}

static uint16_t
m380_ded_read16(uint32_t addr, void *priv)
{
    (void) priv;
    return *(uint16_t *) &ram[0xa0000 + (addr - 0x800a0000)];
}

static uint32_t
m380_ded_read32(uint32_t addr, void *priv)
{
    (void) priv;
    return *(uint32_t *) &ram[0xa0000 + (addr - 0x800a0000)];
}

static void
m380_ded_write8(uint32_t addr, uint8_t val, void *priv)
{
    (void) priv;
    ram[0xa0000 + (addr - 0x800a0000)] = val;
}

static void
m380_ded_write16(uint32_t addr, uint16_t val, void *priv)
{
    (void) priv;
    *(uint16_t *) &ram[0xa0000 + (addr - 0x800a0000)] = val;
}

static void
m380_ded_write32(uint32_t addr, uint32_t val, void *priv)
{
    (void) priv;
    *(uint32_t *) &ram[0xa0000 + (addr - 0x800a0000)] = val;
}

static void
m380_update_shadow(uint8_t reg2)
{
    mem_set_mem_state_both(0xe0000, 0x20000,
                           ((reg2 & 0x40) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) | MEM_WRITE_INTERNAL);
    flushmmucache();
}

/* Called by the keyboard controller for A8h 80h register writes. */
void
machine_at_olivetti_m380_kbc_reg_write(uint8_t reg, uint8_t val)
{
    if (reg == 2)
        m380_update_shadow(val);
}

int
machine_at_olivetti_m380_init(const machine_t *model)
{
    int ret;

    /* Two 27C256 EPROMs form a 64 KiB byte-interleaved image. PBUQ supplies
       even bytes and PBUZ odd bytes. The reset vector is EA 5B E0 00 F0. */
    ret = bios_load_interleaved(
        "roms/machines/olivetti_m380/Olivetti M380 BIOS Version 1_09 PBUQ.BIN",
        "roms/machines/olivetti_m380/Olivetti M380 BIOS Version 1_09 PBUZ.BIN",
        0x000f0000, 65536, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_init(model);

    /* CAP7 p. 7-3 maps the first MiB as 640 KiB conventional RAM followed
       by video memory, VGA ROM/option space, video BIOS shadow and system
       BIOS shadow. */
    /* The hole RAM is the separately reported dedicated memory, see above;
       it is not added to extended memory. */
    mem_mapping_add(&m380_dedicated_mapping, 0x800a0000, 0x60000,
                    m380_ded_read8, m380_ded_read16, m380_ded_read32,
                    m380_ded_write8, m380_ded_write16, m380_ded_write32,
                    NULL, 0, NULL);
    m380_update_shadow(0x00);

    /* The physical machine connects its floppy controller through the IF614
       and the proprietary backplane. Treat 86Box's "internal" selection as
       that controller board until its Olivetti glue logic is implemented. */
    if (fdc_current[0] == FDC_INTERNAL)
        device_add(&fdc_at_device);

    /* CAP7 p. 7-4: two fixed 16450-compatible serial interfaces.  Make them
       motherboard resources instead of relying on the optional standalone
       ports, so COM1 is 3F8h/IRQ4 and COM2 is 2F8h/IRQ3 in every M380. */
    com_ports[0].enabled = 1;
    com_ports[1].enabled = 1;

    serial_t *com1 = device_add_inst(&ns16450_device, 1);
    serial_setup(com1, COM1_ADDR, COM1_IRQ);

    serial_t *com2 = device_add_inst(&ns16450_device, 2);
    serial_setup(com2, COM2_ADDR, COM2_IRQ);

    /* No extra, generic UARTs belong to the documented motherboard. */
    serial_set_next_inst(SERIAL_MAX - 1);

    /* CAP7, p. 7-4 documents two motherboard parallel interfaces: LPT1 at
       378h on IRQ 7 and LPT2 at 278h on IRQ 5.  Their Olivetti glue logic is
       not yet known, so model each as a fixed, standard parallel interface.
       They are not optional add-in cards, therefore reserve both ports here
       rather than relying on the global standalone-port defaults. */
    lpt_ports[0].enabled = 1;
    lpt_ports[1].enabled = 1;

    lpt_t *lpt1 = device_add_inst(&lpt_port_device, 1);
    lpt_port_setup(lpt1, LPT1_ADDR);
    lpt_port_irq(lpt1, LPT1_IRQ);

    lpt_t *lpt2 = device_add_inst(&lpt_port_device, 2);
    lpt_port_setup(lpt2, LPT2_ADDR);
    lpt_port_irq(lpt2, LPT2_IRQ);

    /* Do not let the standalone initializer create further, generic ports. */
    lpt_set_next_inst(PARALLEL_MAX - 1);

    return ret;
}
