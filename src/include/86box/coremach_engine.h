/*
 * BluMach engine selection for a machine configuration.
 *
 * A configuration with a [CoreMach] section is run by the CoreMach engine;
 * the 86Box core never loads, changes or saves it.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef EMU_COREMACH_ENGINE_H
#define EMU_COREMACH_ENGINE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLUMACH_COREMACH_SECTION    "CoreMach"
#define BLUMACH_COREMACH_SYSTEM_MAX 64

typedef enum blumach_engine {
    BLUMACH_ENGINE_86BOX = 0,
    BLUMACH_ENGINE_COREMACH
} blumach_engine_t;

/* Engine of the running machine and, for CoreMach, its configured system
   (empty when the [CoreMach] section does not name one). */
extern blumach_engine_t blumach_engine;
extern char             blumach_coremach_system[BLUMACH_COREMACH_SYSTEM_MAX];

/* Reads the configuration file without changing it. A missing or unreadable
   file selects 86Box, as it does for config_load(). The system name is
   truncated to fit and is empty when absent. */
extern blumach_engine_t blumach_detect_engine(const char *cfg_path, char *system, size_t system_size);

#ifdef __cplusplus
}
#endif

#endif /*EMU_COREMACH_ENGINE_H*/
