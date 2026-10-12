/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008-2025 Sarah Walker
 * Copyright 2016-2025 Miran Grca
 * Copyright 2017-2025 Fred N. van Kempen
 * Copyright 2020 EngiNerd
 * Copyright 2025 Jasmine Iwanek
 * Copyright 2026 rtzor
 * Copyright 2026 BluMach contributors
 *
 * Four-level palette/luminance ported selectively from CoreMach:
 * systems/olivetti-m15/src/olivetti_m15.c, m15_video_render,
 * commit 65361d458ebbe2879613097bdb8a0f3584f1227a.
 * The eight-level extension is a BluMach presentation approximation, not
 * a measurement of the backlit M15 Plus panel. No emulated registers change.
 */
#ifndef BLUMACH_M15_LCD_PALETTE_H
#define BLUMACH_M15_LCD_PALETTE_H

#include <stdint.h>

static inline uint32_t
v6355_m15_lcd_palette_colour(unsigned levels, uint8_t index)
{
    /* The approved M15 reflective LCD: olive background, cool blue-grey ink.
       RGBI brightness darkens the panel rather than producing phosphor glow. */
    static const uint32_t lcd4[4] = {
        UINT32_C(0xff8aa21d), UINT32_C(0xff70892d),
        UINT32_C(0xff4e6e6b), UINT32_C(0xff31535a)
    };
    /* Dark-to-light, interpolated along the same four-colour curve at seven
       equal intervals; computed once, not interpolated for each pixel. */
    static const uint32_t lcd8[8] = {
        UINT32_C(0xff31535a), UINT32_C(0xff3d5f61),
        UINT32_C(0xff4a6a69), UINT32_C(0xff587659),
        UINT32_C(0xff66813f), UINT32_C(0xff748d2b),
        UINT32_C(0xff7f9724), UINT32_C(0xff8aa21d)
    };
    /* Preserve the M15 Plus diagnostic's BLACK-to-WHITE sequence
       7,3,1,F,B,9,8,0 and the existing approximation for other RGBI codes. */
    static const uint8_t m15plus_level[16] = {
        7, 2, 4, 1, 5, 4, 2, 0, 6, 5, 2, 4, 3, 2, 0, 3
    };
    unsigned luminance;

    index &= 0x0f;
    if (levels == 8)
        return lcd8[m15plus_level[index]];

    luminance = ((index & 0x04) ? 3U : 0U) +
                ((index & 0x02) ? 6U : 0U) +
                ((index & 0x01) ? 1U : 0U) +
                ((index & 0x08) ? 3U : 0U);
    return lcd4[(luminance * 3U + 6U) / 13U];
}

#endif
