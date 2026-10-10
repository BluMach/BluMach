/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Host presentation controls. Devices declare capabilities; no guest state.
 */
#ifndef BLUMACH_VIDEO_PANEL_H
#define BLUMACH_VIDEO_PANEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define VIDEO_PANEL_COUNT 8
enum { VIDEO_PANEL_NONE, VIDEO_PANEL_LCD, VIDEO_PANEL_PLASMA };
enum { VIDEO_PANEL_CONTRAST = 1, VIDEO_PANEL_BRIGHTNESS = 2, VIDEO_PANEL_BACKLIGHT = 4 };
enum { VIDEO_PANEL_ACTIVE = 0x100, VIDEO_PANEL_KIND_SHIFT = 16 };
void video_panel_register(int monitor, int kind, unsigned capabilities);
void video_panel_unregister(int monitor);
void video_panel_set_active(int monitor, int active);
unsigned video_panel_description(int monitor);
uint32_t video_panel_settings(int monitor);
uint32_t video_panel_render_settings(int monitor);
void video_panel_load(int monitor, int contrast, int brightness, int backlight);
int video_panel_set(int monitor, unsigned control, int value);
int video_panel_get(int monitor, unsigned control);
void video_panel_restore(int monitor);
/* Called only when rebuilding a device-owned palette, never by Qt. */
uint32_t video_panel_adjust(uint32_t colour, uint32_t settings, int kind);
#ifdef __cplusplus
}
#endif
#endif
