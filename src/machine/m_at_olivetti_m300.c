/*
 * BluMach, a preservation-focused fork of 86Box.
 *
 *          Olivetti M300-02/PCS 11 (BA013/16), M300-02F/PCS 33
 *          (BA013/25), M300-08
 *          (BA319/BA324/BA325), M300-15 (BA320), M300-30 and M300-30P.
 *
 * Author: rtzor
 * Project: BluMach
 *
 * Copyright 2026 rtzor.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/io.h>
#include <86box/keyboard.h>
#include <86box/mem.h>
#include <86box/timer.h>
#include <86box/nvr.h>
#include <86box/port_6x.h>
#include <86box/rom.h>
#include <86box/hdc.h>
#include <86box/sio.h>
#include <86box/video.h>
#include <86box/machine.h>

static void
m300_diag_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) priv;

#ifdef ENABLE_OLIVETTI_M300_LOG
    pclog("Olivetti M300 diagnostic code: %02X\n", val);
#else
    (void) val;
#endif
}

static void
ba013_diag_write(uint16_t port, uint8_t val, void *priv)
{
    m300_diag_write(port, val, priv);

    if (val == 0x62)
        port_6x_topcat_refresh_enable();
    if (val == 0x63)
        port_6x_topcat_refresh_disable();
}

/*
 * BA08 (M290 SP) PIT access time.
 *
 * Resident Diagnostics 1.08 times each PIT channel at POST code 43h
 * (F000:5AE5-5BE0): it loads the counter with FFFFh and counts loop passes,
 * each with one latch command and two counter reads, until the count drops
 * to 40h.  Fewer than about 1000 or more than about 7900 passes send POST to
 * the error routine before video is initialised, so the screen stays black.
 * Without extra bus time the emulated 286 at 20 MHz makes more than 8000
 * passes.  The value below adds the slow AT-bus access of the TOPCAT board
 * to each PIT port access and puts the count near the middle of the window.
 * It is a calibration against the firmware check, not a measured timing.
 */
#define M290SP_PIT_WAIT 20

/*
 * Resident Diagnostics 1.08 checks the refresh toggle (port 61h bit 4) at
 * POST code 44h (F000:5C33), before the memory refresh test that the BA013
 * BIOSes bracket with codes 62h/63h.  Start the TOPCAT refresh model there
 * too; codes 62h and 63h keep their BA013 meaning.
 */
static void
m290sp_diag_write(uint16_t port, uint8_t val, void *priv)
{
    ba013_diag_write(port, val, priv);

    if (val == 0x44)
        port_6x_topcat_refresh_enable();
}

static uint8_t
m290sp_pit_wait_read(uint16_t port, void *priv)
{
    (void) port;
    (void) priv;

    cycles -= M290SP_PIT_WAIT;
    return 0xff;
}

static void
m290sp_pit_wait_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) val;
    (void) priv;

    cycles -= M290SP_PIT_WAIT;
}

static uint8_t
m300_pit_wait_read(uint16_t port, void *priv)
{
    (void) port;
    (void) priv;

    cycles -= 5;
    return 0xff;
}

static void
m300_pit_wait_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) val;
    (void) priv;

    cycles -= 5;
}

static uint8_t
m300_cmos_wait_read(uint16_t port, void *priv)
{
    (void) port;
    (void) priv;

    cycles -= 160;
    return 0xff;
}

static void
m300_cmos_wait_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) val;
    (void) priv;

    cycles -= 160;
}

/*
 * M300-30/30P board configuration register.
 *
 * Resident Diagnostics 1.03, 1.04 and 2.00 open it with the Olivetti KBC
 * command 8Bh, access it through port 70h and close it again with 8Ah
 * (F000:8E43 write, F000:8E97 read in Diagnostics 2.00).  While it is open,
 * port 70h does not reach the RTC index.
 *
 * Read (board straps), as used by the firmware:
 *   bits 2-0: external bus speed code.  Diagnostics 1.03/1.04 decode
 *             2 = 16 MHz, 3 = 20 MHz, 4 = 25 MHz and 5 = 33 MHz (F000:4D19,
 *             F000:17C3) and name a DX2 by its bus (DX2-25MHz).
 *             Diagnostics 2.00 only tests bit 0: 1 = 33 MHz, 0 = 25 MHz
 *             (F000:4D35), and names a DX2 by its core (DX2-50/DX2-66).
 *             Both readings agree for codes 4 and 5;
 *   bit 1:    Diagnostics 2.00 moves the Super I/O configuration index to
 *             26Eh when set (398h when clear); clear in codes 4 and 5;
 *   bit 3:    returned by the far service at F000:8F88; meaning unknown.
 * Write (copy kept at 0040:0891h): bit 0 speed select, together with
 * VL82C486 register 0Ch; bits 1 and 2 are switched by the firmware but
 * their function is not known.
 *
 * Without this register the firmware read back the last RTC index written,
 * so the bus speed and the Super I/O base depended on previous accesses.
 */
static int     m30030_cfg_open;
static uint8_t m30030_cfg_ctrl;
static nvr_t  *m30030_nvr;

static uint8_t
m30030_cfg_read(uint16_t port, void *priv)
{
    uint8_t ret = 0xff;

    (void) port;
    (void) priv;

    if (m30030_cfg_open) {
        if (cpu_busspeed >= 30000000.0)
            ret = 0x05; /* 33 MHz */
        else if (cpu_busspeed >= 22000000.0)
            ret = 0x04; /* 25 MHz */
        else if (cpu_busspeed >= 18000000.0)
            ret = 0x03; /* 20 MHz */
        else
            ret = 0x02; /* 16 MHz */
    }

    return ret;
}

static void
m30030_cfg_write(uint16_t port, uint8_t val, void *priv)
{
    (void) port;
    (void) priv;

    if (m30030_cfg_open)
        m30030_cfg_ctrl = val;
}

void
machine_at_olivetti_m30030_cfg_window(int open)
{
    open = !!open;

    if (open == m30030_cfg_open)
        return;

    m30030_cfg_open = open;

    if (m30030_nvr == NULL)
        return;

    if (open) {
        /* Port 70h goes to the board register; keep 71h on the RTC. */
        nvr_at_handler(0, 0x0070, m30030_nvr);
        nvr_at_data_port(1, m30030_nvr);
    } else {
        nvr_at_data_port(0, m30030_nvr);
        nvr_at_handler(1, 0x0070, m30030_nvr);
    }
}

static int
machine_at_olivetti_m300_init(const machine_t *model, const char *bios_path)
{
    int ret;

    ret = bios_load_linear(bios_path, 0x000f0000, 65536, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_ide_init(model);

    device_add((model->init == machine_at_olivetti_m30008_init) ?
               &opti283_m30008_device : &opti283_m30015_device);
    device_add_params(machine_get_kbc_device(machine),
                      (void *) model->kbc_params);
    device_add(&pc87310_device);

    /* Resident Diagnostics publishes the current test on LPT1 data. */
    io_sethandler(0x0378, 1, NULL, NULL, NULL,
                  m300_diag_write, NULL, NULL, NULL);

    /* Buffered motherboard timings measured from Resident Diagnostics. */
    io_sethandler(0x0040, 4,
                  m300_pit_wait_read, NULL, NULL,
                  m300_pit_wait_write, NULL, NULL, NULL);
    io_sethandler(0x0070, 2,
                  m300_cmos_wait_read, NULL, NULL,
                  m300_cmos_wait_write, NULL, NULL, NULL);

    if (gfxcard[0] == VID_INTERNAL)
        device_add(&oti067_m300_device);

    return ret;
}

int
machine_at_olivetti_m30008_init(const machine_t *model)
{
    return machine_at_olivetti_m300_init(model,
                                         "roms/machines/m30008/BIOS.ROM");
}

int
machine_at_olivetti_m30015_init(const machine_t *model)
{
    return machine_at_olivetti_m300_init(model,
                                         "roms/machines/m30015/BIOS.ROM");
}

static const device_config_t olivetti_m30030_config[] = {
    // clang-format off
    {
        .name           = "bios",
        .description    = "BIOS Version",
        .type           = CONFIG_BIOS,
        .default_string = "diag104",
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = {
            {
                .name          = "Resident Diagnostics 1.03 (09/24/92) + OVC 1.09",
                .internal_name = "diag103",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/m30030/BIOS-1.09-DIAG-1.03.BIN", "" }
            },
            {
                .name          = "Resident Diagnostics 1.04 (11/05/92) + OVC 1.09",
                .internal_name = "diag104",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/m30030/BIOS-1.09-DIAG-1.04.BIN", "" }
            },
            {
                .name          = "Resident Diagnostics 2.00 (02/22/93) + OVC 1.09",
                .internal_name = "diag200",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/m30030/BIOS-1.09-DIAG-2.00.BIN", "" }
            },
            { .files_no = 0 }
        }
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

const device_t olivetti_m30030_device = {
    .name          = "Olivetti M300-30 / M300-30P",
    .internal_name = "olivetti_m30030",
    .flags         = 0,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = olivetti_m30030_config
};

int
machine_at_olivetti_m30030_init(const machine_t *model)
{
    int         ret;
    const char *fn;

    device_context(model->device);
    fn  = device_get_bios_file(machine_get_device(machine),
                               device_get_config_bios("bios"), 0);
    ret = bios_load_linear(fn, 0x000e0000, 131072, 0);
    device_context_restore();

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    /* VL82C486 memory and AT-bus controller used by the BA356 family. */
    device_add(&vl82c486_device);

    /* BA356 has dedicated backing for video/system shadow.  Do not expose the
       split DRAM hole again above installed RAM: Diagnostics otherwise counts
       a false extra 256 KB and overwrites the shadow during its memory test. */
    mem_remap_top_nomid(0);

    /* The firmware times the motherboard refresh signal at port 61h bit 4. */
    device_add(&port_6x_vlsi_refresh_device);

    device_add_params(machine_get_kbc_device(machine),
                      (void *) model->kbc_params);

    /* Buffered ISA IDE and PC87311/87312-compatible Super I/O. */
    device_add(&ide_isa_device);
    device_add_params(&pc873xx_device,
                      (void *) (PCX73XX_IDE_PRI | PCX730X_398));

    io_sethandler(0x0378, 1, NULL, NULL, NULL,
                  m300_diag_write, NULL, NULL, NULL);

    /* Board configuration register behind KBC commands 8Bh/8Ah. */
    m30030_cfg_open = 0;
    m30030_cfg_ctrl = 0x00;
    m30030_nvr      = (nvr_t *) device_get_priv(machines[machine].nvr_device);
    io_sethandler(0x0070, 1,
                  m30030_cfg_read, NULL, NULL,
                  m30030_cfg_write, NULL, NULL, NULL);

    if (gfxcard[0] == VID_INTERNAL)
        device_add(&paradise_wd90c31_m30030_device);

    return ret;
}

static const device_config_t olivetti_m30002_config[] = {
    // clang-format off
    {
        .name           = "bios",
        .description    = "BIOS Version",
        .type           = CONFIG_BIOS,
        .default_string = "m30002_r102",
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = {
            {
                .name          = "Resident Diagnostics 1.02 (04/28/92)",
                .internal_name = "m30002_r102",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/m30002/BIOS-R1.02.ROM", "" }
            },
            { .files_no = 0 }
        }
    },
    {
        .name           = "keylock_locked",
        .description    = "Front-panel key lock",
        .type           = CONFIG_BINARY,
        .default_string = NULL,
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

static const device_config_t olivetti_m30002f_config[] = {
    // clang-format off
    {
        .name           = "bios",
        .description    = "BIOS Version",
        .type           = CONFIG_BIOS,
        .default_string = "m30002f_r501",
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = {
            {
                .name          = "Resident Diagnostics 5.01 (08/24/92)",
                .internal_name = "m30002f_r501",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/m30002f/BIOS.ROM", "" }
            },
            { .files_no = 0 }
        }
    },
    {
        .name           = "keylock_locked",
        .description    = "Front-panel key lock",
        .type           = CONFIG_BINARY,
        .default_string = NULL,
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

static const device_config_t olivetti_pcs11_config[] = {
    // clang-format off
    {
        .name           = "bios",
        .description    = "BIOS Version",
        .type           = CONFIG_BIOS,
        .default_string = "pcs11_r207",
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = {
            {
                .name          = "Resident Diagnostics 2.04 (06/03/92)",
                .internal_name = "pcs11_r204",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/pcs11/BIOS-R2.04.ROM", "" }
            },
            {
                .name          = "Resident Diagnostics 2.07 (03/01/93)",
                .internal_name = "pcs11_r207",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/pcs11/BIOS-R2.07.ROM", "" }
            },
            { .files_no = 0 }
        }
    },
    {
        .name           = "keylock_locked",
        .description    = "Front-panel key lock",
        .type           = CONFIG_BINARY,
        .default_string = NULL,
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

static const device_config_t olivetti_pcs33_config[] = {
    // clang-format off
    {
        .name           = "bios",
        .description    = "BIOS Version",
        .type           = CONFIG_BIOS,
        .default_string = "pcs33_r603",
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = {
            {
                .name          = "Resident Diagnostics 6.03 (03/01/93)",
                .internal_name = "pcs33_r603",
                .bios_type     = BIOS_NORMAL,
                .files_no      = 1,
                .local         = 0,
                .size          = 131072,
                .files         = { "roms/machines/pcs33/BIOS-R6.03.ROM", "" }
            },
            { .files_no = 0 }
        }
    },
    {
        .name           = "keylock_locked",
        .description    = "Front-panel key lock",
        .type           = CONFIG_BINARY,
        .default_string = NULL,
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

const device_t olivetti_m30002_device = {
    .name          = "Olivetti M300-02 (BA013/16)",
    .internal_name = "olivetti_m30002",
    .flags         = 0,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = olivetti_m30002_config
};

const device_t olivetti_m30002f_device = {
    .name          = "Olivetti M300-02F (BA013/25)",
    .internal_name = "olivetti_m30002f",
    .flags         = 0,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = olivetti_m30002f_config
};

const device_t olivetti_pcs11_device = {
    .name          = "Olivetti PCS 11 (BA013/16)",
    .internal_name = "olivetti_pcs11",
    .flags         = 0,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = olivetti_pcs11_config
};

const device_t olivetti_pcs33_device = {
    .name          = "Olivetti PCS 33 (BA013/25)",
    .internal_name = "olivetti_pcs33",
    .flags         = 0,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = olivetti_pcs33_config
};

static int
machine_at_olivetti_ba013_init(const machine_t *model)
{
    int         ret;
    const char *fn;

    /*
     * The lower 64 KiB of the 27C010 contain OVC; the upper 64 KiB contain
     * Resident Diagnostics and the system BIOS.  OVC is called directly at
     * E000:3000 and is therefore not a separate C000 option ROM.
     */
    device_context(model->device);
    fn  = device_get_bios_file(machine_get_device(machine),
                               device_get_config_bios("bios"), 0);
    ret = bios_load_linear(fn, 0x000e0000, 131072, 0);
    device_context_restore();

    if (bios_only || !ret)
        return ret;

    machine_at_common_ide_init(model);

    device_add(&vlsi_topcat_device);
    device_add(&port_6x_topcat_device);
    device_add_params(machine_get_kbc_device(machine),
                      (void *) model->kbc_params);
    device_add(&pc87310_device);

    io_sethandler(0x0378, 1, NULL, NULL, NULL,
                  ba013_diag_write, NULL, NULL, NULL);

    if (gfxcard[0] == VID_INTERNAL)
        device_add(&paradise_wd90c11_ba013_device);

    return ret;
}

int
machine_at_olivetti_m290sp_init(const machine_t *model)
{
    int ret;

    ret = bios_load_linear("roms/machines/m290sp/BIOS-1.08.ROM",
                           0x000e0000, 131072, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_ide_init(model);
    device_add(&vlsi_topcat_ba08_device);
    device_add(&port_6x_topcat_device);
    device_add_params(machine_get_kbc_device(machine),
                      (void *) model->kbc_params);
    device_add(&pc87310_device);
    io_sethandler(0x0378, 1, NULL, NULL, NULL,
                  m290sp_diag_write, NULL, NULL, NULL);
    io_sethandler(0x0040, 4,
                  m290sp_pit_wait_read, NULL, NULL,
                  m290sp_pit_wait_write, NULL, NULL, NULL);

    /* BA08-specific WD90C11 power-on state and embedded font. */
    if (gfxcard[0] == VID_INTERNAL)
        device_add(&paradise_wd90c11_m290sp_device);

    return ret;
}

int
machine_at_olivetti_m30002_init(const machine_t *model)
{
    return machine_at_olivetti_ba013_init(model);
}

int
machine_at_olivetti_m30002f_init(const machine_t *model)
{
    return machine_at_olivetti_ba013_init(model);
}

int
machine_at_olivetti_pcs11_init(const machine_t *model)
{
    return machine_at_olivetti_ba013_init(model);
}

int
machine_at_olivetti_pcs33_init(const machine_t *model)
{
    return machine_at_olivetti_ba013_init(model);
}
