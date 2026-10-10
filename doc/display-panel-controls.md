# Device-owned panel controls

The View menu offers a modeless Screen tool only when a device registers an
internal LCD or plasma panel. Device capabilities decide which controls appear;
Qt does not test manufacturer or product IDs. No generic CRT is registered.

The C interface in `video_panel.h` holds atomic host settings per monitor.
The emulation thread snapshots them and rebuilds its private colour palette
only when a setting changes. UI code does not call a device or edit guest RAM,
registers, DMA, timing or firmware. No new full-frame filter is required.

Settings persist in the VM's `Monitor #N` section as `panel_contrast`,
`panel_brightness` (0–100, default 50) and `panel_backlight` (default 1).
Neutral settings preserve all original RGB values bit for bit. Closing the
tool saves settings; Restore defaults repairs even an unreadable extreme.

LCD contrast approximates the documented whole-panel darkening and loss of
legibility at extremes. Optional backlight OFF reduces illumination but is not
a panel-power switch. Plasma is emissive: never offer backlight for plasma.
Its host gain is independent of BIOS/PDC intensity tables and external CRTs.
Curves are explicitly visual approximations, not calibrated voltage responses.

Guest keys are released before opening the tool. Its arrow keys stay in the
host controls. The tool is nonmodal and does not intentionally pause emulation;
the user's existing pause-on-focus preferences still apply. A panel disabled by
firmware keeps its settings, but controls are disabled until it is active again.

Tests cover capabilities, ranges, independent monitors, disappearance, neutral
colours, LCD washout/backlight, plasma hue and live Qt widgets in five locales.
They do not establish physical panel calibration or guest diagnostic acceptance.
