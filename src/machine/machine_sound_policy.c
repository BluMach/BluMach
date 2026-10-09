/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 Project BluMach.
 * Model-owned audio expansion policy, independent of internal bus compatibility.
 */
#include <stddef.h>
#include <stdint.h>
#include <86box/device.h>
#include <86box/machine.h>
#include <86box/sound.h>

int
machine_has_sound_expansion(int m)
{
    return !machines[m].no_sound_expansion;
}

int
machine_allows_sound_card(int m, int card, int slot)
{
    if (card < 0 || slot < 0 || slot >= SOUND_CARD_MAX)
        return 0;
    if (machine_has_sound_expansion(m) || card == 0)
        return 1;
    /* Fixed onboard audio, if declared, is not an expansion card. The PC
       speaker is not a sound-card slot and is unaffected by this policy. */
    return card == 1 && slot == 0 && (machines[m].flags & MACHINE_SOUND);
}
