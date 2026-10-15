/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach. */
#include <stdint.h>
#include "vx0_clock.h"

extern uint64_t xt_cpu_multi;
uint64_t vx0_clock_period;
uint32_t vx0_clock_phase;
static uint32_t selected_hz;
static double timer_domain_hz;

void
vx0_clock_reset(void)
{
    selected_hz = 0;
    timer_domain_hz = 0;
    vx0_clock_phase = 0;
    vx0_clock_period = xt_cpu_multi;
}

void
vx0_clock_rebase(double timer_hz)
{
    timer_domain_hz = timer_hz;
    vx0_clock_period = selected_hz ?
        (uint64_t) (timer_hz * 4294967296.0 / selected_hz) : xt_cpu_multi;
}

void
vx0_clock_set(uint32_t cpu_hz)
{
    /* Only a rate selection: no CPU/FPU reset, timer rebasing, instruction
     * recognition or mutation of the configured CPU table. */
    if (selected_hz == cpu_hz)
        return;
    selected_hz = cpu_hz;
    vx0_clock_rebase(timer_domain_hz);
}

uint32_t
vx0_clock_get(uint32_t configured_hz)
{
    return selected_hz ? selected_hz : configured_hz;
}
