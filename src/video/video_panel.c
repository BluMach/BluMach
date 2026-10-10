/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Each monitor has independent, atomic host controls. No device pointers or
 * callbacks cross the UI/emulation boundary. Defaults preserve exact RGBs.
 */
#include <stdatomic.h>
#include <86box/video_panel.h>
#define DEFAULT_SETTINGS (50U | (50U << 8) | (1U << 16))
static atomic_uint descriptions[VIDEO_PANEL_COUNT];
/* Zero means not loaded yet; valid packed settings carry bit 24. */
static atomic_uint settings[VIDEO_PANEL_COUNT];
static int valid(int m) { return m >= 0 && m < VIDEO_PANEL_COUNT; }
static int normalized(int v) { return v >= 0 && v <= 100 ? v : 50; }
void video_panel_register(int m, int kind, unsigned caps)
{
    if (valid(m) && (kind == VIDEO_PANEL_LCD || kind == VIDEO_PANEL_PLASMA))
        atomic_store(&descriptions[m], (unsigned)kind << VIDEO_PANEL_KIND_SHIFT |
                     (caps & 7U) | VIDEO_PANEL_ACTIVE);
}
void video_panel_unregister(int m) { if (valid(m)) atomic_store(&descriptions[m], 0); }
void video_panel_set_active(int m, int active)
{
    if (!valid(m)) return;
    unsigned old = atomic_load(&descriptions[m]), next;
    do { next = active ? old | VIDEO_PANEL_ACTIVE : old & ~VIDEO_PANEL_ACTIVE; }
    while (!atomic_compare_exchange_weak(&descriptions[m], &old, next));
}
unsigned video_panel_description(int m) { return valid(m) ? atomic_load(&descriptions[m]) : 0; }
uint32_t video_panel_settings(int m)
{
    unsigned value = valid(m) ? atomic_load(&settings[m]) : 0;
    return value & (1U << 24) ? value & 0x1ffffU : DEFAULT_SETTINGS;
}
void video_panel_load(int m, int contrast, int brightness, int backlight)
{
    if (valid(m)) atomic_store(&settings[m], (1U << 24) |
        (unsigned)normalized(contrast) | ((unsigned)normalized(brightness) << 8) |
        ((unsigned)!!backlight << 16));
}
int video_panel_get(int m, unsigned control)
{
    unsigned v = video_panel_settings(m);
    switch (control) {
        case VIDEO_PANEL_CONTRAST: return v & 127U;
        case VIDEO_PANEL_BRIGHTNESS: return (v >> 8) & 127U;
        case VIDEO_PANEL_BACKLIGHT: return (v >> 16) & 1U;
        default: return -1;
    }
}
int video_panel_set(int m, unsigned control, int value)
{
    unsigned description = video_panel_description(m);
    if (!valid(m) || !(description & VIDEO_PANEL_ACTIVE) || !(description & control) ||
        value < 0 || value > (control == VIDEO_PANEL_BACKLIGHT ? 1 : 100)) return 0;
    unsigned shift;
    if (control == VIDEO_PANEL_CONTRAST) shift = 0;
    else if (control == VIDEO_PANEL_BRIGHTNESS) shift = 8;
    else if (control == VIDEO_PANEL_BACKLIGHT) shift = 16;
    else return 0;
    unsigned mask = (control == VIDEO_PANEL_BACKLIGHT ? 1U : 127U) << shift;
    unsigned old = atomic_load(&settings[m]), next;
    do {
        unsigned base = old & (1U << 24) ? old : DEFAULT_SETTINGS;
        next = (1U << 24) | (base & ~mask) | ((unsigned)value << shift);
    } while (!atomic_compare_exchange_weak(&settings[m], &old, next));
    return 1;
}
void video_panel_restore(int m)
{
    if (valid(m)) atomic_store(&settings[m], (1U << 24) | DEFAULT_SETTINGS);
}
static unsigned mix(unsigned a, unsigned b, unsigned weight)
{ return (a * (100U - weight) + b * weight + 50U) / 100U; }
uint32_t video_panel_adjust(uint32_t colour, uint32_t state, int kind)
{
    unsigned c = state & 127U, b = (state >> 8) & 127U;
    unsigned channels[3] = { (colour >> 16) & 255U, (colour >> 8) & 255U, colour & 255U };
    const unsigned light[3] = { 0x8a, 0xa2, 0x1d }, dark[3] = { 0x31, 0x53, 0x5a };
    if (kind != VIDEO_PANEL_LCD && kind != VIDEO_PANEL_PLASMA) return colour;
    c = c > 100 ? 100 : c; b = b > 100 ? 100 : b;
    for (unsigned i = 0; i < 3; ++i) {
        if (kind == VIDEO_PANEL_LCD) {
            /* The documented LCD knob darkens the whole panel; both extremes
               lose readability. This curve is visual, not voltage calibrated. */
            if (c < 50) channels[i] = mix(light[i], channels[i], c * 2U);
            else if (c > 50) channels[i] = mix(channels[i], dark[i], (c - 50U) * 2U);
            if (!(state & (1U << 16))) channels[i] = channels[i] * 55U / 100U;
        } else {
            /* Plasma is emissive, not backlit. Contrast and brightness retain
               the orange hue and keep black black. Neutral is bit-identical. */
            unsigned value = channels[i] * c * b / 2500U;
            channels[i] = value > 255 ? 255 : value;
        }
    }
    return (colour & 0xff000000U) | (channels[0] << 16) | (channels[1] << 8) | channels[2];
}
