/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <86box/machine.h>

const machine_t machines[] = {
    { .flags = MACHINE_FLAGS_NONE },
    { .no_sound_expansion = 1 },
    { .no_sound_expansion = 1, .flags = MACHINE_SOUND }
};

int main(void)
{
    assert(machine_has_sound_expansion(0));
    assert(!machine_has_sound_expansion(1));
    for (int slot = 0; slot < 4; ++slot) {
        assert(machine_allows_sound_card(0, 7, slot));
        assert(machine_allows_sound_card(1, 0, slot));
        assert(!machine_allows_sound_card(1, 7, slot));
        assert(!machine_allows_sound_card(1, 1, slot));
        assert(!machine_allows_sound_card(2, 7, slot));
    }
    assert(machine_allows_sound_card(2, 1, 0));
    assert(!machine_allows_sound_card(2, 1, 1));
    assert(!machine_allows_sound_card(0, -1, 0));
    assert(!machine_allows_sound_card(0, 0, -1));
    assert(!machine_allows_sound_card(0, 0, 4));
    return 0;
}
