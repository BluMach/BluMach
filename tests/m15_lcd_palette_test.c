/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Tests the exact presentation function used by V6355D LCD scanout.
 * No firmware, media, GUI or analogue panel measurements required.
 */
#include <assert.h>
#include <stdint.h>
#include "../src/video/vid_m15_lcd_palette.h"
#include <86box/video_panel.h>

static unsigned
luminance(uint32_t colour)
{
    return 3U * ((colour >> 16) & 255U) +
           6U * ((colour >> 8) & 255U) + (colour & 255U);
}

int
main(void)
{
    const uint32_t approved[4] = {
        0xff8aa21dU, 0xff70892dU, 0xff4e6e6bU, 0xff31535aU
    };
    const uint8_t m15_level[16] = {
        0, 0, 1, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 2, 3, 3
    };
    const uint32_t plus[8] = {
        0xff31535aU, 0xff3d5f61U, 0xff4a6a69U, 0xff587659U,
        0xff66813fU, 0xff748d2bU, 0xff7f9724U, 0xff8aa21dU
    };
    const uint8_t plus_level[16] = {
        7, 2, 4, 1, 5, 4, 2, 0, 6, 5, 2, 4, 3, 2, 0, 3
    };
    const uint8_t diagnostic[8] = { 7, 3, 1, 15, 11, 9, 8, 0 };

    for (unsigned index = 0; index < 256; ++index) {
        const uint32_t base = v6355_m15_lcd_palette_colour(4, (uint8_t) index);
        const uint32_t enhanced = v6355_m15_lcd_palette_colour(8, (uint8_t) index);
        assert(base == approved[m15_level[index & 15]]);
        assert(enhanced == plus[plus_level[index & 15]]);
        assert((base >> 24) == 255U && (enhanced >> 24) == 255U);
        assert((base & 0xffffffU) != 0U && (enhanced & 0xffffffU) != 0U);
    }
    assert(v6355_m15_lcd_palette_colour(4, 0) == approved[0]);
    assert(v6355_m15_lcd_palette_colour(4, 15) == approved[3]);
    video_panel_load(0, 50, 50, 1);
    video_panel_register(0, VIDEO_PANEL_LCD, VIDEO_PANEL_CONTRAST);
    for (unsigned index = 0; index < 16; ++index) {
        const uint32_t colour = v6355_m15_lcd_palette_colour(4, index);
        assert(video_panel_adjust(colour, video_panel_settings(0), VIDEO_PANEL_LCD) == colour);
    }
    assert(video_panel_set(0, VIDEO_PANEL_CONTRAST, 75));
    assert(video_panel_adjust(approved[0], video_panel_settings(0), VIDEO_PANEL_LCD) != approved[0]);
    assert(!video_panel_set(0, VIDEO_PANEL_BACKLIGHT, 0));
    video_panel_restore(0);
    video_panel_register(1, VIDEO_PANEL_LCD, VIDEO_PANEL_CONTRAST | VIDEO_PANEL_BACKLIGHT);
    video_panel_restore(1);
    assert(video_panel_set(1, VIDEO_PANEL_BACKLIGHT, 0));
    assert(video_panel_get(0, VIDEO_PANEL_BACKLIGHT) == 1);
    for (unsigned level = 0; level < 8; ++level) {
        assert(v6355_m15_lcd_palette_colour(8, diagnostic[level]) == plus[level]);
        if (level > 0)
            assert(luminance(plus[level]) > luminance(plus[level - 1]));
    }
    return 0;
}
