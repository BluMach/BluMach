/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/port_92.h>

const device_t port_92_device = { 0 };
int            refresh_at_enable;

static void    *io_context;
static uint8_t (*read_port)(uint16_t, void *);
static void    (*write_port)(uint16_t, uint8_t, void *);
static uint16_t shadow_c;
static uint16_t shadow_d;

void *
device_add(const device_t *device)
{
    assert(device == &port_92_device);
    return NULL;
}

void
io_sethandler(uint16_t base, uint16_t size,
              uint8_t (*inb)(uint16_t, void *),
              uint16_t (*inw)(uint16_t, void *),
              uint32_t (*inl)(uint16_t, void *),
              void (*outb)(uint16_t, uint8_t, void *),
              void (*outw)(uint16_t, uint16_t, void *),
              void (*outl)(uint16_t, uint32_t, void *), void *context)
{
    assert(base == 0x22 && size == 2);
    assert(inw == NULL && inl == NULL && outw == NULL && outl == NULL);
    read_port  = inb;
    write_port = outb;
    io_context = context;
}

void
mem_set_access(uint8_t bitmap, int mode, uint32_t base, uint32_t size, uint16_t access)
{
    assert(bitmap == ACCESS_ALL && mode == 0 && size == 0x10000);
    if (base == 0xc0000)
        shadow_c = access;
    else if (base == 0xd0000)
        shadow_d = access;
}

void
flushmmucache_nopc(void)
{
}

static void
write_reg(uint8_t index, uint8_t value)
{
    write_port(0x22, index, io_context);
    write_port(0x23, value, io_context);
    assert(read_port(0x23, io_context) == value);
}

int
main(void)
{
    const uint16_t external   = MEM_READ_EXTANY | MEM_WRITE_EXTANY;
    const uint16_t read_only  = MEM_READ_INTERNAL | MEM_WRITE_EXTANY;
    const uint16_t read_write = MEM_READ_INTERNAL | MEM_WRITE_INTERNAL;
    void          *chipset    = et6000_device.init(&et6000_device);

    assert(chipset != NULL && chipset == io_context);

    write_reg(0x14, 0x20);
    write_reg(0x15, 0x0f);
    assert(shadow_c == read_write && shadow_d == read_write);

    write_reg(0x14, 0x00);
    assert(shadow_c == external && shadow_d == external);
    write_reg(0x14, 0x20);
    assert(shadow_c == read_write && shadow_d == read_write);

    write_reg(0x15, 0x0a);
    assert(shadow_c == read_only && shadow_d == read_only);
    write_reg(0x15, 0x03);
    assert(shadow_c == read_write && shadow_d == external);
    write_reg(0x15, 0x0c);
    assert(shadow_c == external && shadow_d == read_write);

    et6000_device.close(chipset);
    return 0;
}
