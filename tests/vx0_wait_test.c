/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * The actual BIU wait must accept 8087 latencies exceeding 255 cycles. */
#include <assert.h>
#include <stdint.h>
#include <86box/plat_unused.h>
static int calls;
static int expected;
static void biu_cycle(void)
{
    ++calls;
    /* Fail a wrapped counter promptly instead of hanging the test runner. */
    assert(calls <= expected);
}
#define vx0_biu_log(...) ((void)0)
#include "vx0_wait.inc"

int main(void)
{
    const int requests[] = {-1, 0, 1, 2, 254, 255, 256, 285, 570, 10000};
    for (unsigned i=0; i<sizeof(requests)/sizeof(requests[0]); ++i) {
        calls = 0;
        expected = requests[i] > 0 ? requests[i] : 0;
        wait_vx0(requests[i], 0);
        assert(calls == expected);
    }
    return 0;
}
