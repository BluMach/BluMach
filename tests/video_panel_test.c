/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach. No firmware or guest media.
 */
#include <assert.h>
#include <86box/video_panel.h>
int main(void)
{
    assert(video_panel_description(-1) == 0 && video_panel_description(8) == 0);
    assert(!video_panel_set(0, VIDEO_PANEL_CONTRAST, 40));
    video_panel_load(0, 50, 50, 1);
    video_panel_register(0, VIDEO_PANEL_LCD, VIDEO_PANEL_CONTRAST);
    assert(!video_panel_set(0, VIDEO_PANEL_BACKLIGHT, 0));
    assert(!video_panel_set(0, VIDEO_PANEL_CONTRAST, -1));
    assert(!video_panel_set(0, VIDEO_PANEL_CONTRAST, 101));
    assert(!video_panel_set(0, VIDEO_PANEL_CONTRAST | VIDEO_PANEL_BRIGHTNESS, 50));
    assert(video_panel_adjust(0xff8aa21dU, video_panel_settings(0), VIDEO_PANEL_LCD) == 0xff8aa21dU);
    assert(video_panel_adjust(0xff31535aU, video_panel_settings(0), VIDEO_PANEL_LCD) == 0xff31535aU);
    video_panel_register(1, VIDEO_PANEL_LCD, VIDEO_PANEL_CONTRAST | VIDEO_PANEL_BACKLIGHT);
    video_panel_restore(1);
    assert(video_panel_set(1, VIDEO_PANEL_BACKLIGHT, 0));
    assert(video_panel_get(0, VIDEO_PANEL_BACKLIGHT) == 1);
    assert(video_panel_adjust(0xff8aa21dU, video_panel_settings(1), VIDEO_PANEL_LCD) != 0xff8aa21dU);
    assert(video_panel_set(1, VIDEO_PANEL_CONTRAST, 0));
    assert(video_panel_adjust(0xff31535aU, video_panel_settings(1), VIDEO_PANEL_LCD) ==
           video_panel_adjust(0xff8aa21dU, video_panel_settings(1), VIDEO_PANEL_LCD));
    video_panel_set_active(1, 0);
    assert(!video_panel_set(1, VIDEO_PANEL_CONTRAST, 70));
    video_panel_set_active(1, 1);
    video_panel_restore(1);
    assert(video_panel_get(1, VIDEO_PANEL_CONTRAST) == 50);
    video_panel_unregister(1);
    assert(video_panel_description(1) == 0);
    video_panel_register(1, VIDEO_PANEL_PLASMA, VIDEO_PANEL_CONTRAST | VIDEO_PANEL_BRIGHTNESS);
    assert(video_panel_adjust(0xffe06018U, video_panel_settings(1), VIDEO_PANEL_PLASMA) == 0xffe06018U);
    assert(!video_panel_set(1, VIDEO_PANEL_BACKLIGHT, 0));
    assert(video_panel_set(1, VIDEO_PANEL_BRIGHTNESS, 25));
    assert(video_panel_adjust(0xffe06018U, video_panel_settings(1), VIDEO_PANEL_PLASMA) == 0xff70300cU);
    assert(video_panel_adjust(0xff000000U, video_panel_settings(1), VIDEO_PANEL_PLASMA) == 0xff000000U);
    assert(video_panel_adjust(0xff123456U, video_panel_settings(1), VIDEO_PANEL_NONE) == 0xff123456U);
    video_panel_load(1, -100, 500, 0);
    assert(video_panel_get(1, VIDEO_PANEL_CONTRAST) == 50 && video_panel_get(1, VIDEO_PANEL_BRIGHTNESS) == 50);
    return 0;
}
