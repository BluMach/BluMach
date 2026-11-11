/* BluMach memory mapping regression. GPL-2.0-or-later.
 * Copyright 2026 rtzor, Project BluMach.
 * Includes the production recalculation; no firmware or emulated CPU needed.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <86box/mem.h>

static mem_mapping_t *base_mapping;
mem_mapping_t *read_mapping[MEM_MAPPINGS_NO], *write_mapping[MEM_MAPPINGS_NO];
static mem_mapping_t *read_mapping_bus[MEM_MAPPINGS_NO], *write_mapping_bus[MEM_MAPPINGS_NO];
uint8_t *_mem_exec[MEM_MAPPINGS_NO];
static uint8_t _mem_wp[MEM_MAPPINGS_NO], _mem_wp_bus[MEM_MAPPINGS_NO];
static mem_state_t _mem_state[MEM_MAPPINGS_NO];
static int cpu_16bitbus = 1, is6117, in_smm, is_cxsmm;
static uint8_t ccr1;
#define CCR1_SMAC (1 << 2) /* CPU state is a bounded test double. */
static unsigned flushes;
void flushmmucache_nopc(void) { ++flushes; }
#include "mem_mapping_recalc.inc"

static uint8_t read_byte(uint32_t addr, void *priv) { (void)addr; (void)priv; return 0; }
static void write_byte(uint32_t addr, uint8_t val, void *priv) { (void)addr; (void)val; (void)priv; }

static void reset_tables(void)
{
    base_mapping = NULL;
    memset(read_mapping, 0, sizeof(read_mapping));
    memset(write_mapping, 0, sizeof(write_mapping));
    memset(read_mapping_bus, 0, sizeof(read_mapping_bus));
    memset(write_mapping_bus, 0, sizeof(write_mapping_bus));
    memset(_mem_exec, 0, sizeof(_mem_exec));
}

static mem_mapping_t mapping(uint32_t base, uint32_t size, uint8_t *exec)
{
    mem_mapping_t map = {0};
    map.enable = 1;
    map.base = base;
    map.size = size;
    map.read_b = read_byte;
    map.write_b = write_byte;
    map.exec = exec;
    return map;
}

static void expect_page(uint32_t addr, mem_mapping_t *map, uint8_t *exec)
{
    const unsigned page = addr >> MEM_GRANULARITY_BITS;
    assert(read_mapping[page] == map);
    assert(write_mapping[page] == map);
    assert(read_mapping_bus[page] == map);
    assert(write_mapping_bus[page] == map);
    assert(_mem_exec[page] == exec);
}

static void test_interior_interval(void)
{
    static uint8_t backing[0x10000], left_exec[0x1000], right_exec[0x1000];
    mem_mapping_t parent = mapping(0x10000, sizeof(backing), backing);
    mem_mapping_t left = mapping(0x14000, sizeof(left_exec), left_exec);
    mem_mapping_t right = mapping(0x16000, sizeof(right_exec), right_exec);
    reset_tables();
    base_mapping = &parent;
    parent.next = &left;
    left.next = &right;
    mem_mapping_recalc(parent.base, parent.size, 0);

    /* Rebuild only the middle 4 KiB of the larger mapping. A preceding
       overlay ends exactly at the interval; a following one starts at its end.
       Neither may be replaced by the parent's read, write or execute route. */
    mem_mapping_recalc(0x15000, 0x1000, 0);
    expect_page(0x13000, &parent, backing + 0x3000);
    expect_page(0x14000, &left, left_exec);
    expect_page(0x15000, &parent, backing + 0x5000);
    expect_page(0x16000, &right, right_exec);
    expect_page(0x17000, &parent, backing + 0x7000);
}

static void test_top_of_address_space(void)
{
    static uint8_t backing[0x3000], neighbor_exec[0x1000], last_exec[0x1000];
    mem_mapping_t parent = mapping(0xffffd000u, sizeof(backing), backing);
    mem_mapping_t neighbor = mapping(0xffffe000u, sizeof(neighbor_exec), neighbor_exec);
    mem_mapping_t last = mapping(0xfffff000u, sizeof(last_exec), last_exec);
    reset_tables();
    cpu_16bitbus = 0;
    is6117 = 0;
    base_mapping = &parent;
    parent.next = &neighbor;
    neighbor.next = &last;
    mem_mapping_recalc(parent.base, parent.size, 0);
    expect_page(0xfffff000u, &last, last_exec);

    /* The exclusive endpoint is 0x100000000, not a wrapped 32-bit zero.
       Falling back in the last page must preserve both preceding pages. */
    last.enable = 0;
    mem_mapping_recalc(0xfffff000u, 0x1000, 0);
    expect_page(0xffffd000u, &parent, backing);
    expect_page(0xffffe000u, &neighbor, neighbor_exec);
    expect_page(0xfffff000u, &parent, backing + 0x2000);
    last.enable = 1;
    mem_mapping_recalc(0xfffff000u, 0x1000, 0);
    expect_page(0xffffd000u, &parent, backing);
    expect_page(0xffffe000u, &neighbor, neighbor_exec);
    expect_page(0xfffff000u, &last, last_exec);
}

static void test_data_aliases(void)
{
    static uint8_t outside_exec[0x1000];
    const uint64_t masks[] = {0x00ffffffu, 0x03ffffffu, 0xffffffffu};
    const uint32_t ignore = 0xff000000u;
    const uint64_t stride = 0x01000000u;
    for (unsigned bus = 0; bus < 3; ++bus) {
        mem_mapping_t parent = mapping(0x20000, 0x4000, NULL);
        mem_mapping_t left = mapping(0x20000, 0x1000, NULL);
        mem_mapping_t right = mapping(0x22000, 0x1000, NULL);
        mem_mapping_t outside = {0};
        reset_tables();
        cpu_16bitbus = bus != 2;
        is6117 = bus == 1;
        base_mapping = &parent;
        parent.next = &left;
        left.next = &right;
        parent.base_ignore = left.base_ignore = right.base_ignore = ignore;
        const uint64_t last_alias = masks[bus] & ignore;
        for (uint64_t alias = 0; alias <= last_alias; alias += stride) {
            /* Sentinels immediately outside every replica catch overrun. */
            const unsigned before = (unsigned)((alias + 0x1f000) >> MEM_GRANULARITY_BITS);
            const unsigned after = (unsigned)((alias + 0x24000) >> MEM_GRANULARITY_BITS);
            read_mapping[before] = write_mapping[before] = &outside;
            read_mapping_bus[before] = write_mapping_bus[before] = &outside;
            read_mapping[after] = write_mapping[after] = &outside;
            read_mapping_bus[after] = write_mapping_bus[after] = &outside;
            _mem_exec[before] = _mem_exec[after] = outside_exec;
        }
        mem_mapping_recalc(parent.base, parent.size, ignore);
        mem_mapping_recalc(0x21000, 0x1000, ignore);
        for (uint64_t alias = 0; alias <= last_alias; alias += stride) {
            expect_page((uint32_t)(alias + 0x1f000), &outside, outside_exec);
            expect_page((uint32_t)(alias + 0x20000), &left, NULL);
            expect_page((uint32_t)(alias + 0x21000), &parent, NULL);
            expect_page((uint32_t)(alias + 0x22000), &right, NULL);
            expect_page((uint32_t)(alias + 0x23000), &parent, NULL);
            expect_page((uint32_t)(alias + 0x24000), &outside, outside_exec);
        }
        if (last_alias + stride < UINT64_C(0x100000000))
            expect_page((uint32_t)(last_alias + stride + parent.base), NULL, NULL);
        /* These are data-only aliases. Direct-execute alias offset semantics
           are a separate existing issue and are deliberately not blessed here. */
    }
}

int main(void)
{
    static uint8_t backing[0xa0000], page_exec[4][0x4000];
    mem_mapping_t low = {0}, window[4] = {{0}};
    low.enable = 1; low.base = 0; low.size = sizeof(backing);
    low.read_b = read_byte; low.write_b = write_byte; low.exec = backing;
    base_mapping = &low;
    for (unsigned i=0; i<4; ++i) {
        window[i].enable=1; window[i].base=0x10000+i*0x4000; window[i].size=0x4000;
        window[i].read_b=read_byte; window[i].write_b=write_byte;
        window[i].exec=page_exec[i];
        if(i) window[i-1].next=&window[i]; else low.next=&window[i];
    }
    mem_mapping_recalc(0, 0xa0000, 0);
    for (unsigned changed=0; changed<4; ++changed) {
        /* Updating one page must preserve every sibling, both CPU and DMA. */
        mem_mapping_recalc(window[changed].base, window[changed].size, 0);
        for (unsigned i=0; i<4; ++i) {
            unsigned page=window[i].base>>12;
            assert(read_mapping[page]==&window[i]);
            assert(write_mapping[page]==&window[i]);
            assert(read_mapping_bus[page]==&window[i]);
            assert(write_mapping_bus[page]==&window[i]);
            assert(_mem_exec[page]==page_exec[i]);
        }
    }
    window[3].enable=0;
    mem_mapping_recalc(window[3].base, window[3].size, 0);
    assert(read_mapping[0x1c]==&low);
    assert(_mem_exec[0x1c]==backing+0x1c000);
    for(unsigned i=0;i<3;++i) assert(read_mapping[window[i].base>>12]==&window[i]);
    /* A partial update must also leave subsequent addresses alone. */
    window[0].enable=0;
    mem_mapping_recalc(window[0].base, window[0].size, 0);
    assert(read_mapping[0x10]==&low);
    assert(read_mapping[0x14]==&window[1]);
    assert(flushes==7);
    test_interior_interval();
    test_top_of_address_space();
    test_data_aliases();
    puts("memory mapping range: PASS");
    return 0;
}
