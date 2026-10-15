/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * NEC CPU clock selection without changing the peripheral timer domain. */
#ifndef BLUMACH_VX0_CLOCK_H
#define BLUMACH_VX0_CLOCK_H
#include <stdint.h>

extern uint64_t vx0_clock_period;
extern uint32_t vx0_clock_phase;

void vx0_clock_reset(void);
void vx0_clock_rebase(double timer_hz);
void vx0_clock_set(uint32_t cpu_hz);
uint32_t vx0_clock_get(uint32_t configured_hz);

/* Keep the fractional phase across rate changes. Integral legacy periods
 * produce exactly the same TSC increments as before. */
static inline uint64_t
vx0_clock_tick(void)
{
    if ((uint32_t) vx0_clock_period == 0)
        return vx0_clock_period >> 32;
    const uint32_t previous = vx0_clock_phase;
    vx0_clock_phase += (uint32_t) vx0_clock_period;
    return (vx0_clock_period >> 32) + (vx0_clock_phase < previous);
}
#endif
