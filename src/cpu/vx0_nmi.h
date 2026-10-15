/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach. */
#ifndef BLUMACH_VX0_NMI_H
#define BLUMACH_VX0_NMI_H

#include <stdint.h>
#include <86box/nmi.h>

/* Called only after dispatching vector 2 (including a custom vector).
 * Keep the legacy build option consistent with the other interpreters. */
static inline void
vx0_nmi_acknowledge(void)
{
#ifndef OLD_NMI_BEHAVIOR
    nmi = 0;
#endif
}

#endif
