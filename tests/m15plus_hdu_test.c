/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Exercise the real HDU port/state machine with bounded DMA/disk/PIC doubles.
 * No firmware or guest media is used. */
#include <assert.h>
#include "../src/disk/hdc_xta.c"

uint64_t tsc;
uint64_t TIMER_USEC = UINT64_C(1) << 32;
uint8_t dma_e;
uint8_t dma_m = 0xff;
hard_disk_t hdd[HDD_NUM];
static uint8_t disk[68 * SECTOR_SIZE];
static uint8_t host_buffer[SECTOR_SIZE];
static unsigned dma_position, writes, irq_requests, irq_lines;
static unsigned unexpected_config_reads;

void timer_enable(pc_timer_t *t) { t->flags |= TIMER_ENABLED; }
void timer_disable(pc_timer_t *t) { t->flags &= ~TIMER_ENABLED; }
void picint_common(uint16_t v, int mode, int level, uint8_t *priv)
{
    (void)mode; (void)priv;
    assert(v == (1u << 5));
    if (level) { irq_lines |= v; irq_requests++; }
    else irq_lines &= ~v;
}
int dma_channel_read(int channel)
{
    assert(channel == 3);
    if (!(dma_e & 8) || (dma_m & 8)) return DMA_NODATA;
    assert(dma_position < sizeof(host_buffer));
    return host_buffer[dma_position++];
}
int dma_channel_write(int channel, uint16_t value)
{
    assert(channel == 3);
    if (!(dma_e & 8) || (dma_m & 8)) return DMA_NODATA;
    assert(dma_position < sizeof(host_buffer));
    host_buffer[dma_position++] = (uint8_t)value;
    return 0;
}
void ui_sb_update_icon(int type, int value) { (void)type; (void)value; }
void ui_sb_update_icon_write(int type, int value) { (void)type; (void)value; }
int hdd_image_load(int id) { assert(id == 0); return 1; }
void hdd_image_close(uint8_t id) { assert(id == 0); }
/* Report unexpected configuration lookups at the test boundary. Do not make
   these stubs noreturn: GCC also compiles the unused legacy constructors. */
int device_get_config_int(const char *name) { (void)name; unexpected_config_reads++; return 5; }
int device_get_config_hex20(const char *name) { (void)name; unexpected_config_reads++; return 0xc8000; }
const char *device_get_config_bios(const char *name) { (void)name; unexpected_config_reads++; return "rev_1"; }
const char *device_get_bios_file(const device_t *info, const char *name, unsigned file)
{ (void)info; (void)name; (void)file; unexpected_config_reads++; return "unused"; }
int rom_present(const char *path) { (void)path; return 0; }
int rom_init(rom_t *rom, const char *path, uint32_t address, int size, int mask, int offset, uint32_t flags)
{ (void)rom; (void)path; (void)address; (void)size; (void)mask; (void)offset; (void)flags; assert(0); return 0; }
void timer_add(pc_timer_t *t, void (*callback)(void *), void *priv, int start)
{ t->callback = callback; t->priv = priv; t->flags = start ? TIMER_ENABLED : 0; }
#define TEST_IO_ARGUMENTS uint16_t base, uint16_t size, \
    uint8_t (*inb)(uint16_t, void *), uint16_t (*inw)(uint16_t, void *), uint32_t (*inl)(uint16_t, void *), \
    void (*outb)(uint16_t, uint8_t, void *), void (*outw)(uint16_t, uint16_t, void *), \
    void (*outl)(uint16_t, uint32_t, void *), void *priv
void io_sethandler(TEST_IO_ARGUMENTS)
{ assert(base == 0x320 && size == 4 && inb == hdc_read && outb == hdc_write && !inw && !inl && !outw && !outl && priv); }
void io_removehandler(TEST_IO_ARGUMENTS)
{ (void)base; (void)size; (void)inb; (void)inw; (void)inl; (void)outb; (void)outw; (void)outl; (void)priv; }
void io_handler(uint8_t set, TEST_IO_ARGUMENTS)
{ if (set) io_sethandler(base, size, inb, inw, inl, outb, outw, outl, priv); }
#undef TEST_IO_ARGUMENTS
int hdd_image_read(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
    assert(id == 0 && (uint64_t)(sector + count) * SECTOR_SIZE <= sizeof(disk));
    memcpy(buffer, disk + sector * SECTOR_SIZE, count * SECTOR_SIZE);
    return 0;
}
int hdd_image_write(uint8_t id, uint32_t sector, uint32_t count, uint8_t *buffer)
{
    assert(id == 0 && (uint64_t)(sector + count) * SECTOR_SIZE <= sizeof(disk));
    memcpy(disk + sector * SECTOR_SIZE, buffer, count * SECTOR_SIZE);
    writes++;
    return 0;
}

static hdc_t controller(int type)
{
    hdc_t d = {0};
    d.type = type; d.base = 0x320; d.irq = 5; d.dma = 3;
    d.drives[0].present = 1;
    d.drives[0].spt = d.drives[0].cfg_spt = 17;
    d.drives[0].hpc = d.drives[0].cfg_hpc = 4;
    d.drives[0].tracks = d.drives[0].cfg_tracks = 615;
    return d;
}
static void tick(hdc_t *d)
{
    assert(timer_is_enabled(&d->timer));
    tsc = d->timer.ts_integer;
    timer_disable(&d->timer);
    hdc_callback(d);
}
static void command(hdc_t *d, uint8_t op, unsigned lba)
{
    uint8_t bytes[6] = {op, (uint8_t)(lba / 17), (uint8_t)(lba % 17), 0, 1, 0};
    dma_position = 0;
    hdc_write(0x323, DMA_ENA | IRQ_ENA, d);
    hdc_write(0x322, 0, d);
    for (unsigned i = 0; i < sizeof(bytes); i++) {
        assert((hdc_read(0x321, d) & 15) == 13);
        hdc_write(0x320, bytes[i], d);
    }
    tick(d);
}
static void finish_dma(hdc_t *d)
{
    unsigned previous_irqs = irq_requests;
    /* BIOS 1.10 at F000:AC63 waits for this before waiting for IRQ5. */
    assert(hdc_read(0x321, d) & STAT_DRQ);
    assert(!(hdc_read(0x321, d) & STAT_IRQ));
    tick(d);
    assert(dma_position == SECTOR_SIZE);
    tick(d);
    assert(irq_requests == previous_irqs + 1);
    assert((hdc_read(0x321, d) & (STAT_IRQ | STAT_DRQ)) == STAT_IRQ);
    assert(hdc_read(0x320, d) == 0);
    assert(hdc_read(0x321, d) == 0);
}
int main(void)
{
    hdd[0].bus_type = HDD_BUS_XTA;
    hdd[0].spt = 17; hdd[0].hpc = 4; hdd[0].tracks = 615;
    hdc_t *created = xta_olivetti_m15plus_device.init(&xta_olivetti_m15plus_device);
    assert(created->type == 8 && created->base == 0x320 && created->dma == 3 && created->irq == 5);
    assert(created->drives[0].present && !created->bios_rom.rom);
    assert(unexpected_config_reads == 0);
    hdc_t d = *created;
    free(created);
    /* Channel 3 starts masked, as in the M15 Plus BIOS setup. The HDU gate
       enables it; reset subsequently cancels and masks a pending transfer. */
    memset(disk, 0xcc, sizeof(disk));
    memset(host_buffer, 0, sizeof(host_buffer));
    for (unsigned lba = 0; lba < 68; lba++) {
        command(&d, CMD_WRITE_SECTORS, lba);
        assert((dma_e & 8) && !(dma_m & 8));
        finish_dma(&d);
    }
    assert(writes == 68);
    for (unsigned i = 0; i < sizeof(disk); i++) assert(disk[i] == 0);
    /* Partition-sector pattern and readback through the same port interface. */
    for (unsigned i = 0; i < sizeof(host_buffer); i++) host_buffer[i] = (uint8_t)(i * 13 + 7);
    host_buffer[510] = 0x55; host_buffer[511] = 0xaa;
    command(&d, CMD_WRITE_SECTORS, 0); finish_dma(&d);
    memset(host_buffer, 0, sizeof(host_buffer));
    command(&d, CMD_READ_SECTORS, 0); finish_dma(&d);
    assert(!memcmp(host_buffer, disk, sizeof(host_buffer)));
    assert(host_buffer[510] == 0x55 && host_buffer[511] == 0xaa);
    command(&d, CMD_WRITE_SECTORS, 1);
    unsigned previous_writes = writes;
    hdc_write(0x321, 0, &d);
    assert(!timer_is_enabled(&d.timer) && d.state == STATE_IDLE);
    assert(hdc_read(0x321, &d) == 0 && irq_lines == 0 && (dma_m & 8));
    assert(writes == previous_writes && d.drives[0].present);
    command(&d, CMD_READ_SECTORS, 0); finish_dma(&d);
    /* No new DRQ status or DMA gate behaviour for the PCS86 controller. */
    hdc_t pcs = controller(6);
    dma_e = 0; dma_m = 0xff;
    command(&pcs, CMD_READ_SECTORS, 0);
    assert(!(hdc_read(0x321, &pcs) & STAT_DRQ));
    assert(dma_e == 0 && dma_m == 0xff);
    puts("PASS M15 Plus DMA-ready DRQ, IRQ completion, first-cylinder writes, partition-sector readback, reset cancellation/recovery, PCS86 isolation");
    return 0;
}
