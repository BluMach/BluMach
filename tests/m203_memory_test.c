/* BluMach modifications: rtzor, Project BluMach, 2026. */
/* Production M203 translations, mapping APIs and instruction-fetch cache.
 * CPU state and backing-page dirty tracking are bounded test doubles.
 * No BIOS, disk, or copied chipset implementation is included.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <86box/mem.h>

#define TEST_RAM_SIZE 0x500000
static uint8_t storage[TEST_RAM_SIZE], test_rom[0x20000];
static page_t test_pages[TEST_RAM_SIZE >> 12];
uint8_t *ram = storage;
page_t *pages = test_pages;
uint32_t mem_size;
uint32_t mem_logical_addr, rammask = 0xffffff;
mem_mapping_t ram_low_mapping, ram_mid_mapping, ram_high_mapping, ram_remapped_mapping;
static mem_mapping_t rom_mapping;
static mem_mapping_t *base_mapping, *last_mapping;
mem_mapping_t *read_mapping[MEM_MAPPINGS_NO], *write_mapping[MEM_MAPPINGS_NO];
static mem_mapping_t *read_mapping_bus[MEM_MAPPINGS_NO], *write_mapping_bus[MEM_MAPPINGS_NO];
uint8_t *_mem_exec[MEM_MAPPINGS_NO];
static uint8_t _mem_wp[MEM_MAPPINGS_NO], _mem_wp_bus[MEM_MAPPINGS_NO];
static mem_state_t _mem_state[MEM_MAPPINGS_NO];
static int cpu_16bitbus = 1, is6117, in_smm, is_cxsmm, is286 = 1;
static uint8_t ccr1, ff_pccache[4] = {0xff, 0xff, 0xff, 0xff};
static uint32_t cr0;
static int cpu_prefetch_cycles, cpu_rom_prefetch_cycles, cpu_mem_prefetch_cycles;
#define CCR1_SMAC (1 << 2)
#define mem_log(...) do { } while (0)
#define mmutranslate_read(addr) (addr) /* 286 has no paging. */
#define OLIMCU_LOG(...) do { } while (0)
#define OLIMCU_MEM_LOG(...) do { } while (0)
static unsigned checks;
#define CHECK(value) do { ++checks; assert(value); } while (0)

void fatal(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args);
    abort();
}
void flushmmucache_nopc(void) { }
void flushmmucache(void) { }
void addreadlookup(uint32_t logical, uint32_t physical) { (void)logical; (void)physical; }
void addwritelookup(uint32_t logical, uint32_t physical) { (void)logical; (void)physical; }
void mem_write_ramb_page(uint32_t addr, uint8_t value, page_t *page)
{
    assert(addr < (uint32_t)mem_size << 10);
    assert(page == &pages[addr >> 12]);
    ram[addr] = value;
}
static uint8_t bios_read(uint32_t addr, void *priv)
{
    (void)priv;
    return test_rom[addr - 0xe0000];
}
static void olivetti_pcs286s_apply_timing(uint8_t value) { (void)value; }
#include "mem_mapping_recalc.inc"
#include "m203_mapping_api.inc"
#include "m203_getpccache.inc"
#include "m203_memory_core.inc"
#include "m203_memory_registers.inc"

static uint8_t ram_read(uint32_t addr, void *priv) { (void)priv; return ram[addr]; }
static void ram_write(uint32_t addr, uint8_t value, void *priv)
{
    (void)priv;
    ram[addr] = value;
}

static void fixture(unsigned kib)
{
    const size_t used = (TEST_RAM_SIZE + 0x60000) >> 12;
    memset(read_mapping, 0, used * sizeof(*read_mapping));
    memset(write_mapping, 0, used * sizeof(*write_mapping));
    memset(read_mapping_bus, 0, used * sizeof(*read_mapping_bus));
    memset(write_mapping_bus, 0, used * sizeof(*write_mapping_bus));
    memset(_mem_exec, 0, used * sizeof(*_mem_exec));
    memset(_mem_state, 0, used * sizeof(*_mem_state));
    memset(&ram_low_mapping, 0, sizeof(ram_low_mapping));
    memset(&ram_mid_mapping, 0, sizeof(ram_mid_mapping));
    memset(&ram_high_mapping, 0, sizeof(ram_high_mapping));
    memset(&ram_remapped_mapping, 0, sizeof(ram_remapped_mapping));
    memset(&rom_mapping, 0, sizeof(rom_mapping));
    memset(&olivetti_pcs286s_olimcu, 0, sizeof(olivetti_pcs286s_olimcu));
    base_mapping = last_mapping = NULL;
    mem_size = kib;
    cpu_mem_prefetch_cycles = 2;
    cpu_rom_prefetch_cycles = 4;
    for (unsigned i = 0; i < kib * 1024; ++i)
        ram[i] = (uint8_t)((i >> 12) ^ (i >> 16) ^ i);
    for (unsigned i = 0; i < sizeof(test_rom); ++i)
        test_rom[i] = (uint8_t)(0x67 ^ i);
    mem_set_mem_state_both(0, TEST_RAM_SIZE + 0x60000,
                           MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
    mem_mapping_add(&ram_low_mapping, 0, 0xa0000,
                    ram_read, NULL, NULL, ram_write, NULL, NULL, ram,
                    MEM_MAPPING_INTERNAL, NULL);
    mem_mapping_add(&ram_mid_mapping, 0xa0000, 0x60000,
                    ram_read, NULL, NULL, ram_write, NULL, NULL, ram + 0xa0000,
                    MEM_MAPPING_INTERNAL, NULL);
    mem_mapping_add(&rom_mapping, 0xe0000, 0x20000,
                    bios_read, NULL, NULL, NULL, NULL, NULL, test_rom,
                    MEM_MAPPING_EXTERNAL | MEM_MAPPING_ROM | MEM_MAPPING_ROMCS, NULL);
    olivetti_pcs286s_olimcu_t *dev = &olivetti_pcs286s_olimcu;
    dev->m203 = 1;
    memset(dev->open_bus_exec, 0xff, sizeof(dev->open_bus_exec));
    olivetti_pcs286s_olimcu_banks_init(dev, 0x100000);
    for (unsigned slot = 0; slot < OLIMCU_WINDOWS; ++slot) {
        olivetti_pcs286s_olimcu_window_t *window = &dev->window[slot];
        window->dev = dev; window->logical = slot * OLIMCU_PAGE_SIZE;
        mem_mapping_add(&window->mapping, window->logical, OLIMCU_PAGE_SIZE,
                        olivetti_pcs286s_olimcu_mem_read8, olivetti_pcs286s_olimcu_mem_read16,
                        olivetti_pcs286s_olimcu_mem_read32, olivetti_pcs286s_olimcu_mem_write8,
                        olivetti_pcs286s_olimcu_mem_write16, olivetti_pcs286s_olimcu_mem_write32,
                        NULL, MEM_MAPPING_INTERNAL, window);
        mem_mapping_disable(&window->mapping);
    }
    mem_mapping_add(&dev->regs_mapping, 0xff000, 0x1000,
                    olivetti_pcs286s_olimcu_reg_read, olivetti_pcs286s_olimcu_reg_read16,
                    olivetti_pcs286s_olimcu_reg_read32, olivetti_pcs286s_olimcu_reg_write,
                    olivetti_pcs286s_olimcu_reg_write16, olivetti_pcs286s_olimcu_reg_write32,
                    NULL, 0, dev);
    for (unsigned slot = 0; slot < M203_EXT_SEGMENTS; ++slot) {
        olivetti_pcs286s_olimcu_window_t *window = &dev->extended[slot];
        window->dev = dev; window->linear = 1;
        mem_mapping_add(&window->mapping, 0, 0,
                        olivetti_pcs286s_olimcu_mem_read8, olivetti_pcs286s_olimcu_mem_read16,
                        olivetti_pcs286s_olimcu_mem_read32, olivetti_pcs286s_olimcu_mem_write8,
                        olivetti_pcs286s_olimcu_mem_write16, olivetti_pcs286s_olimcu_mem_write32,
                        NULL, MEM_MAPPING_INTERNAL, window);
    }
    olivetti_m203_shadow_update(dev);
}

static void reg_write(uint8_t index, uint8_t value)
{
    olivetti_pcs286s_olimcu_t *dev = &olivetti_pcs286s_olimcu;
    olivetti_pcs286s_olimcu_reg_write(0xfffee, 0xaa, dev);
    olivetti_pcs286s_olimcu_reg_write(0xfffee, 0x55, dev);
    olivetti_pcs286s_olimcu_reg_write(0xfffec, index, dev);
    olivetti_pcs286s_olimcu_reg_write(0xfffea, value, dev);
    olivetti_pcs286s_olimcu_reg_write(0xfffee, 0xff, dev);
}

static uint8_t read_byte(uint32_t addr)
{
    mem_mapping_t *map = read_mapping[addr >> 12];
    return map && map->read_b ? map->read_b(addr, map->priv) : 0xff;
}
static void write_byte(uint32_t addr, uint8_t value)
{
    mem_mapping_t *map = write_mapping[addr >> 12];
    if (map && map->write_b) map->write_b(addr, value, map->priv);
}
static uint8_t fetch_byte(uint32_t addr)
{
    CHECK(_mem_exec[addr >> 12] != NULL);
    uintptr_t cache = (uintptr_t)getpccache(addr);
    mem_mapping_t *map = read_mapping[addr >> 12];
    CHECK(cpu_prefetch_cycles == ((map && (map->flags & MEM_MAPPING_ROM_WS)) ?
                                 cpu_rom_prefetch_cycles : cpu_mem_prefetch_cycles));
    /* Match fastreadb() in 386_common.h: getpccache rebases the low 32 bits
       and retains the host pointer's upper bits. Plain pointer addition is
       incorrect if that rebase wraps, as with macOS's static test storage. */
    uintptr_t pointer = ((cache + addr) & (uintptr_t)UINT32_MAX) |
                        (cache & ~(uintptr_t)UINT32_MAX);
    return *(uint8_t *)pointer;
}
static void check_ram_alias(uint32_t logical, uint32_t physical)
{
    CHECK(read_byte(logical) == ram[physical]);
    CHECK(fetch_byte(logical) == ram[physical]);
    CHECK(_mem_exec[logical >> 12] == ram + (physical & ~0xfff));
    CHECK(read_mapping[logical >> 12] == read_mapping_bus[logical >> 12]);
    CHECK(write_mapping[logical >> 12] == write_mapping_bus[logical >> 12]);
    uint8_t value = ram[physical] ^ 0x5a;
    write_byte(logical, value);
    CHECK(ram[physical] == value);
    CHECK(fetch_byte(logical) == value);
}

static void test_extended(void)
{
    const unsigned capacities[] = {1024, 3072, 5120};
    const uint8_t shadows[] = {0, 0x24, 0x12, 0x36};
    for (unsigned c = 0; c < 3; ++c) {
        for (unsigned s = 0; s < 4; ++s) {
            fixture(capacities[c]);
            reg_write(0x0e, shadows[s]);
            uint32_t installed = capacities[c] << 10;
            unsigned reserved = (shadows[s] & 0x20 ? 1 : 0) + (shadows[s] & 0x10 ? 1 : 0);
            uint32_t top = installed + 0x60000 - reserved * 0x10000;
            for (uint32_t logical = 0x100000; logical < top; logical += 0x1000) {
                uint32_t physical = logical;
                if (logical >= installed) {
                    physical = logical - installed + 0xa0000;
                    if ((shadows[s] & 0x30) == 0x20 && physical >= 0xe0000)
                        physical += 0x10000;
                }
                check_ram_alias(logical, physical);
                check_ram_alias(logical + 0xfff, physical + 0xfff);
            }
            CHECK(read_mapping[top >> 12] == NULL);
            CHECK(_mem_exec[top >> 12] == NULL);
            /* A reduced 512 KiB boundary must truncate high DRAM without
               retaining any of the old reclaimed executable aliases. */
            uint8_t reduced = (uint8_t)((installed >> 19) - 1);
            uint32_t reduced_top = ((uint32_t)reduced << 19) + 0x60000 - reserved * 0x10000;
            reg_write(0x0f, reduced);
            if (reduced_top > 0x100000)
                check_ram_alias(reduced_top - 1, reduced_top - 1);
            uint32_t unmapped = reduced_top < 0x100000 ? 0x100000 : reduced_top;
            CHECK(read_mapping[unmapped >> 12] == NULL);
            CHECK(read_mapping_bus[unmapped >> 12] == NULL);
            CHECK(_mem_exec[unmapped >> 12] == NULL);
            CHECK(_mem_exec[(top - 1) >> 12] == NULL);
            /* The limit register must remove CPU, DMA and executable aliases. */
            reg_write(0x0f, 1);
            CHECK(read_mapping[0x100] == NULL);
            CHECK(read_mapping_bus[0x100] == NULL);
            CHECK(_mem_exec[0x100] == NULL);
            reg_write(0x0f, (uint8_t)(installed >> 19));
            if (top > 0x100000) check_ram_alias(0x100000, installed == 0x100000 ? 0xa0000 : 0x100000);
        }
    }
}

static void test_shadow(void)
{
    fixture(5120);
    reg_write(0x0e, 0);
    CHECK(read_byte(0xe0010) == test_rom[0x10]);
    CHECK(fetch_byte(0xe0010) == test_rom[0x10]);
    reg_write(0x0e, 6);
    check_ram_alias(0xe0010, 0xe0010);
    check_ram_alias(0xf0010, 0xf0010);
    check_ram_alias(0xff010, 0xff010); /* Register overlay's ordinary bytes. */
    reg_write(0x0e, 0x36);
    uint8_t before = ram[0xe0010];
    write_byte(0xe0010, before ^ 0xff);
    CHECK(ram[0xe0010] == before);
    CHECK(read_byte(0xe0010) == fetch_byte(0xe0010));
    before = ram[0xff010]; write_byte(0xff010, before ^ 0xff);
    CHECK(ram[0xff010] == before);
    CHECK(read_byte(0xff010) == fetch_byte(0xff010));
    reg_write(0x0e, 0);
    CHECK(read_byte(0xff010) == test_rom[0x1f010]);
    CHECK(fetch_byte(0xff010) == test_rom[0x1f010]);
}

static void test_page_frame(void)
{
    fixture(5120); reg_write(0x0a, 0x10); reg_write(0x0c, 4);
    olivetti_pcs286s_olimcu_t *dev = &olivetti_pcs286s_olimcu;
    for (unsigned slot = 0; slot < 4; ++slot) {
        uint16_t port = 0x208 + slot * 0x4000;
        olivetti_pcs286s_olimcu_write(port + 1, 1, dev);
        olivetti_pcs286s_olimcu_write(port, 0x80 + slot, dev);
        check_ram_alias(0x10000 + slot * 0x4000, 0x200000 + slot * 0x4000);
    }
    /* A5 is also a valid enabled page number once the global frame is active. */
    olivetti_pcs286s_olimcu_write(0x208, 0xa5, dev);
    CHECK(olivetti_pcs286s_olimcu_read(0x208, dev) == 0xa5);
    check_ram_alias(0x10000, 0x200000 + 0x25 * 0x4000);
    /* The EMS frame can reach reclaimed RAM while E0000h is shadowed. */
    reg_write(0x0e, 0x24);
    olivetti_pcs286s_olimcu_write(0x209, 2, dev);
    olivetti_pcs286s_olimcu_write(0x208, 0xc0, dev);
    check_ram_alias(0x10000, 0xa0000);
    /* Out-of-range global pages must not fetch conventional RAM underneath. */
    olivetti_pcs286s_olimcu_write(0x209, 0xf, dev);
    olivetti_pcs286s_olimcu_write(0x208, 0xff, dev);
    CHECK(read_byte(0x10000) == 0xff); CHECK(fetch_byte(0x10000) == 0xff);
    CHECK(read_byte(0x13fff) == 0xff); CHECK(fetch_byte(0x13fff) == 0xff);
    olivetti_pcs286s_olimcu_write(0x209, 1, dev);
    olivetti_pcs286s_olimcu_write(0x208, 0xa5, dev);
    /* Moving and disabling the frame restores ordinary low-RAM instruction fetch. */
    reg_write(0x0c, 8); check_ram_alias(0x20000, 0x200000 + 0x25 * 0x4000);
    check_ram_alias(0x10000, 0x10000);
    reg_write(0x0a, 0); check_ram_alias(0x20000, 0x20000);
    /* Bank probing an absent bank must not execute underlying conventional RAM. */
    reg_write(0x0c, 0); reg_write(0x10, 3); reg_write(0x0a, 0x10);
    olivetti_pcs286s_olimcu_write(0x209, 0, dev);
    olivetti_pcs286s_olimcu_write(0x208, 0x80, dev);
    CHECK(read_byte(0) == 0xff); CHECK(fetch_byte(0) == 0xff);
    uint8_t before = ram[0]; write_byte(0, 0x33); CHECK(ram[0] == before);
    /* Width handlers use the same translated backing as byte and fetch access. */
    reg_write(0x10, 1);
    olivetti_pcs286s_olimcu_mem_write32(4, 0x78563412, &dev->window[0]);
    CHECK(olivetti_pcs286s_olimcu_mem_read16(4, &dev->window[0]) == 0x3412);
    CHECK(olivetti_pcs286s_olimcu_mem_read32(4, &dev->window[0]) == 0x78563412);
    CHECK(fetch_byte(4) == 0x12);
}

int main(void)
{
    test_extended(); test_shadow(); test_page_frame();
    printf("M203 RAM/page-frame/shadow data, DMA and instruction fetch: %u checks PASS\n", checks);
    return 0;
}
