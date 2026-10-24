/* Conventional Memories 3inONEder A-form card. SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EMU_CONVENTIONAL_3INONEDER_H
#define EMU_CONVENTIONAL_3INONEDER_H

#include <86box/device.h>

enum conventional_3inoneder_opl_io {
    CONVENTIONAL_3INONEDER_OPL_388 = 0,
    CONVENTIONAL_3INONEDER_OPL_220,
    CONVENTIONAL_3INONEDER_OPL_240,
    CONVENTIONAL_3INONEDER_OPL_388_AND_220,
    CONVENTIONAL_3INONEDER_OPL_DISABLED
};

enum conventional_3inoneder_xtide {
    CONVENTIONAL_3INONEDER_XTIDE_NONE = 0,
    CONVENTIONAL_3INONEDER_XTIDE_AT_INT_300,
    CONVENTIONAL_3INONEDER_XTIDE_AT320INT_320
};

typedef struct {
    const char *machine;
    int         opl_io;
    int         xtide;
    int         joystick;
    int         ethernet;
} conventional_3inoneder_params_t;

extern const device_t conventional_3inoneder_device;

#endif /* EMU_CONVENTIONAL_3INONEDER_H */
