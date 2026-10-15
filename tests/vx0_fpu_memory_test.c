/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Compile the actual V30 qword operand reader against a bounded bus double. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

static int cpu_mod;
static uint32_t easeg;
static struct { uint32_t eaaddr; } cpu_state;
static uint64_t operand;
static unsigned reads;
static uint64_t readmemq(uint32_t segment, uint32_t offset)
{
    assert(segment == 0x49b40 && offset == 0x8cc8);
    ++reads;
    return operand;
}
static void fatal(const char *message)
{
    (void)message;
    assert(0 && "register operands are invalid for this reader");
}
#include "vx0_geteaq.inc"

int main(void)
{
    cpu_mod = 0;
    easeg = 0x49b40;
    cpu_state.eaaddr = 0x8cc8;
    const uint64_t patterns[] = {
        UINT64_C(0x4000000000000000), /* Check-It's divisor, IEEE double 2.0 */
        UINT64_C(0x3ff0000000000000), /* 1.0 */
        UINT64_C(0x8000000000000000), /* negative zero, sign must survive */
        UINT64_C(0x0123456789abcdef), UINT64_MAX, 0
    };
    for (unsigned i = 0; i < sizeof(patterns)/sizeof(patterns[0]); ++i) {
        operand = patterns[i];
        assert(geteaq() == operand);
    }
    assert(reads == 6);
    operand = patterns[0];
    uint64_t bits = geteaq();
    double divisor;
    memcpy(&divisor, &bits, sizeof(divisor));
    assert(divisor == 2.0 && 1.0/divisor == 0.5);
    assert(reads == 7);
    return 0;
}
