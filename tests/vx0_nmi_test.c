/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Pending-request contract; guest Check-It validates CPU integration. */
#include <assert.h>
#include "vx0_nmi.h"

int nmi;
int nmi_mask;
int nmi_auto_clear;

int main(void)
{
    int nmi_enable = 1;
    nmi_mask = 0x80;
    nmi_auto_clear = 0; /* PCS86's 8087 exception path. */
    nmi = 1;
    assert(nmi && nmi_enable && nmi_mask);
    nmi_enable = 0; /* The interpreter inhibits NMI on entry. */
    vx0_nmi_acknowledge();
    assert(nmi_mask == 0x80 && nmi_auto_clear == 0);
    nmi_enable = 1; /* IRET must not replay the delivered request. */
#ifdef OLD_NMI_BEHAVIOR
    assert(nmi && nmi_enable && nmi_mask);
#else
    assert(!(nmi && nmi_enable && nmi_mask));
#endif
    nmi = 1; /* A subsequent exception still requests a fresh NMI. */
    assert(nmi && nmi_enable && nmi_mask);
    vx0_nmi_acknowledge();
#ifndef OLD_NMI_BEHAVIOR
    assert(nmi == 0);
#endif
    nmi = 0;
    vx0_nmi_acknowledge();
    assert(nmi == 0);
    return 0;
}
