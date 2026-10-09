/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 Project BluMach.
 * Read-only validation of the selected firmware, without selecting a fallback.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <86box/device.h>
#include <86box/ini.h>
#include <86box/config.h>

extern int rom_present(const char *);

const device_config_bios_t *
device_configured_bios(const device_t *dev)
{
    char section[256];
    if (!dev || !dev->config)
        return NULL;
    device_get_name(dev, 0, section);
    for (const device_config_t *cfg = dev->config; cfg->type != CONFIG_END; ++cfg) {
        if (cfg->type != CONFIG_BIOS)
            continue;
        const char *selected = config_get_string(section, cfg->name, (char *) cfg->default_string);
        for (const device_config_bios_t *bios = cfg->bios; bios->files_no && bios->internal_name; ++bios)
            if (selected && !strcmp(selected, bios->internal_name))
                return bios;
        return NULL;
    }
    return NULL;
}

int
device_configured_bios_available(const device_t *dev)
{
    if (!dev || !dev->config)
        return 0;
    for (const device_config_t *cfg = dev->config; cfg->type != CONFIG_END; ++cfg) {
        if (cfg->type != CONFIG_BIOS)
            continue;
        const device_config_bios_t *bios = device_configured_bios(dev);
        if (!bios)
            return -1;
        for (int i = 0; i < bios->files_no; ++i)
            if (!rom_present(bios->files[i]))
                return -1;
        return 1;
    }
    return 0;
}
