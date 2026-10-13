/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Original PCS86 utility transactions and fixed-domain clock accounting.
 * No firmware, proprietary executable or guest disk is included. */
#include <assert.h>
#include "../src/machine/m_xt_olivetti_pcs86.c"

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((externally_visible))
#endif
uint64_t xt_cpu_multi;
int cpu;
static const CPU cpu_models[] = {{.rspeed = 10000000}, {.rspeed = 9545456}};
static cpu_family_t family;
cpu_family_t *cpu_f = &family;
static unsigned peripheral_changes;

void xta_handler(void *p, int v) { (void)p; (void)v; peripheral_changes++; }
void paradise_pcs86_set_enabled(void *p, int v) { (void)p; (void)v; peripheral_changes++; }
void lpt_port_setup(lpt_t *p, uint16_t v) { (void)p; (void)v; peripheral_changes++; }
void lpt_port_remove(lpt_t *p) { (void)p; peripheral_changes++; }
void serial_setup(serial_t *p, uint16_t v, uint8_t irq) { (void)p; (void)v; (void)irq; peripheral_changes++; }
void serial_remove(serial_t *p) { (void)p; peripheral_changes++; }
#define IO_ARGS uint16_t b, uint16_t s, uint8_t (*ib)(uint16_t, void *), \
    uint16_t (*iw)(uint16_t, void *), uint32_t (*il)(uint16_t, void *), \
    void (*ob)(uint16_t, uint8_t, void *), void (*ow)(uint16_t, uint16_t, void *), \
    void (*ol)(uint16_t, uint32_t, void *), void *p
void io_sethandler(IO_ARGS)
{ (void)b; (void)s; (void)ib; (void)iw; (void)il; (void)ob; (void)ow; (void)ol; (void)p; peripheral_changes++; }
void io_removehandler(IO_ARGS)
{ (void)b; (void)s; (void)ib; (void)iw; (void)il; (void)ob; (void)ow; (void)ol; (void)p; peripheral_changes++; }
#undef IO_ARGS

static void utility(pcs86_t *d, int fast)
{
    pcs86_board_write(0x6c, 0xda, d);
    uint8_t v = pcs86_glue_read(0x6f, d);
    v = fast ? (v | 0xc0) : (v & 0x3f);
    pcs86_board_write(0x6f, v, d);
    pcs86_board_write(0x6c, 0, d);
}

static uint64_t ticks(uint32_t count)
{
    uint64_t elapsed = 0;
    for (uint32_t i = 0; i < count; i++) elapsed += vx0_clock_tick();
    return elapsed;
}

int main(void)
{
    family.cpus = cpu_models;
    xt_cpu_multi = UINT64_C(3) << 32;
    vx0_clock_reset();
    vx0_clock_rebase(30000000.0);
    pcs86_t d = {0};
    d.control = 0x95;
    d.jumpers = 0x79;
    d.glue[15] = 0xeb;
    assert(vx0_clock_get(10000000) == 10000000);
    assert(ticks(10000000) == 30000000); /* one second, integral legacy fast */

    /* A write outside the observed transaction does not change the clock. */
    pcs86_board_write(0x6f, 0x2b, &d);
    assert(vx0_clock_get(10000000) == 10000000);
    utility(&d, 0);
    assert(vx0_clock_get(10000000) == 7159092);
    assert(d.glue[15] == 0x2b && d.glue[12] == 0);
    /* Same fixed 30 MHz timer domain: one slow CPU second is still one
     * PIT/RTC/video second. Fractional ticks must accumulate, not truncate. */
    uint64_t elapsed = ticks(7159092);
    assert(elapsed >= 29999999 && elapsed <= 30000000);
    const uint32_t phase = vx0_clock_phase;
    utility(&d, 1);
    assert(vx0_clock_phase == phase);
    assert(vx0_clock_get(10000000) == 10000000);
    assert(ticks(10000000) == 30000000);
    assert(d.glue[15] == 0xeb);
    assert(d.control == 0x95 && d.jumpers == 0x79);
    assert(peripheral_changes == 0);

    pcs86_board_write(0x6c, 0xda, &d);
    pcs86_board_write(0x6f, 0x6b, &d); /* unknown mixed pair */
    assert(vx0_clock_get(10000000) == 10000000);
    pcs86_board_write(0x6f, 0xab, &d);
    assert(vx0_clock_get(10000000) == 10000000);
    pcs86_board_write(0x6c, 0, &d);

    /* Existing 9.54 MHz configurations remain configured at that rate;
     * GOFAST restores it rather than silently rewriting the user's VM. */
    cpu = 1;
    vx0_clock_reset();
    xt_cpu_multi = UINT64_C(3) << 32;
    vx0_clock_rebase(28636368.0);
    utility(&d, 0);
    utility(&d, 1);
    assert(vx0_clock_get(9545456) == 9545456);
    assert(ticks(100) == 300);

    /* Reconfiguration clears rate/phase; unrelated NEC machines keep their
     * original integral divider. A software CPU reset does not call this. */
    utility(&d, 0);
    vx0_clock_reset();
    xt_cpu_multi = UINT64_C(2) << 32;
    vx0_clock_rebase(14318184.0);
    assert(vx0_clock_get(7159092) == 7159092 && vx0_clock_phase == 0);
    assert(ticks(7159092) == 14318184);
    return 0;
}
