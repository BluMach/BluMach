/*
 * BluMach bridge to the CoreMach engine.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_HPP
#define QT_COREMACH_HPP

#include <QString>

class BluMachCatalog;

namespace CoreMachBridge {

struct System {
    const char *id;
};

enum class StartupCheck {
    Ready,
    NotBuilt,
    MissingSystem,
    UnknownSystem,
};

// True when this build includes the CoreMach engine.
bool built();
// The CoreMach systems this build runs; nullptr when systemId is not one.
const System *findSystem(const QString &systemId);
// Whether a configuration naming systemId can run in this build.
StartupCheck checkStartup(const QString &systemId);
// Stable identifier of a check result, for logs and automation.
const char *startupCheckCode(StartupCheck check);
// Localised explanation of a failed check.
QString startupMessage(const BluMachCatalog &catalog, StartupCheck check,
                       const QString &machineName, const QString &systemId);

// Runs, or rejects with an error, the CoreMach machine selected by pc_init().
// Never loads or saves an 86Box configuration. Returns the process exit code.
int run();

}

#endif
