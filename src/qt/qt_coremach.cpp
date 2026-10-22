/*
 * BluMach bridge to the CoreMach engine.
 *
 * pc_init() hands a configuration with a [CoreMach] section to this bridge
 * before any 86Box module is initialised. The 86Box core never loads,
 * changes or saves such a configuration.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach.hpp"
#include "qt_blumach_catalog.hpp"

#include <QMessageBox>

#include <cstdio>

extern "C" {
#include <86box/86box.h>
#include <86box/coremach_engine.h>
}

namespace CoreMachBridge {
namespace {
    const System systems[] = {
        { "olivetti-m10" },
    };

    // Startup failures exit with the code used for unavailable firmware.
    constexpr int startupFailureExitCode = 6;

    // BLUMACH_UNATTENDED=1 reports failures on stderr only, for automation.
    bool unattended()
    {
        return qEnvironmentVariable("BLUMACH_UNATTENDED") == QStringLiteral("1");
    }

    int report(const char *code, QMessageBox::Icon icon, const QString &title,
               const QString &text, int exitCode)
    {
        std::fprintf(stderr, "BluMach: CoreMach %s: %s\n", code, text.toUtf8().constData());
        std::fflush(stderr);
        if (!unattended()) {
            QMessageBox box(icon, title, text);
            box.exec();
        }
        return exitCode;
    }

    QString machineName()
    {
        return vm_name[0] ? QString::fromUtf8(vm_name) : QStringLiteral("BluMach");
    }
}

bool
built()
{
#ifdef BLUMACH_COREMACH
    return true;
#else
    return false;
#endif
}

const System *
findSystem(const QString &systemId)
{
    if (!built())
        return nullptr;
    for (const auto &system : systems)
        if (systemId == QLatin1String(system.id))
            return &system;
    return nullptr;
}

StartupCheck
checkStartup(const QString &systemId)
{
    if (!built())
        return StartupCheck::NotBuilt;
    if (systemId.isEmpty())
        return StartupCheck::MissingSystem;
    if (!findSystem(systemId))
        return StartupCheck::UnknownSystem;
    return StartupCheck::Ready;
}

const char *
startupCheckCode(StartupCheck check)
{
    switch (check) {
        case StartupCheck::Ready:
            return "ready";
        case StartupCheck::NotBuilt:
            return "not_built";
        case StartupCheck::MissingSystem:
            return "missing_system";
        case StartupCheck::UnknownSystem:
            return "unknown_system";
    }
    return "unknown";
}

QString
startupMessage(const BluMachCatalog &catalog, StartupCheck check,
               const QString &machineName, const QString &systemId)
{
    switch (check) {
        case StartupCheck::Ready:
            return QString();
        case StartupCheck::NotBuilt:
            return catalog.text(QStringLiteral("coremach.not_built")).arg(machineName);
        case StartupCheck::MissingSystem:
            return catalog.text(QStringLiteral("coremach.missing_system")).arg(machineName);
        case StartupCheck::UnknownSystem:
            return catalog.text(QStringLiteral("coremach.unknown_system")).arg(machineName, systemId);
    }
    return QString();
}

int
run()
{
    BluMachCatalog catalog;
    catalog.load();
    const QString title    = catalog.text(QStringLiteral("firmware.unavailable"));
    const QString systemId = QString::fromUtf8(blumach_coremach_system);

    const auto check = checkStartup(systemId);
    if (check != StartupCheck::Ready)
        return report(startupCheckCode(check), QMessageBox::Warning, title,
                      startupMessage(catalog, check, machineName(), systemId), startupFailureExitCode);

    if (settings_only)
        return report("settings_unavailable", QMessageBox::Information, machineName(),
                      catalog.text(QStringLiteral("coremach.settings_unavailable")).arg(machineName()), 0);

    return report("session_unavailable", QMessageBox::Warning, title,
                  QStringLiteral("CoreMach sessions are not available in this development build yet."),
                  startupFailureExitCode);
}

}
