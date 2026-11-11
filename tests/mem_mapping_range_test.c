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
    puts("memory mapping range: PASS");
    return 0;
}
