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
#include "qt_firmware.hpp"
#ifdef BLUMACH_COREMACH
#    include "qt_coremach_window.hpp"
#    include "qt_vmmanager_clientsocket.hpp"
#endif

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#ifdef Q_OS_WINDOWS
#    include <windows.h>
#endif

#include <cstdio>

extern "C" {
#include <86box/86box.h>
#include <86box/coremach_engine.h>
#include <86box/mem.h>
#include <86box/rom.h>
}

namespace CoreMachBridge {
namespace {
    // Systems this build runs.
    const char *const runnableSystems[] = {
        "olivetti-m10",
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

    QStringList romFolders()
    {
        QStringList folders;
        for (const rom_path_t *path = &rom_paths; path != nullptr; path = path->next)
            if (path->path[0] != '\0')
                folders.append(QString::fromUtf8(path->path));
        return folders;
    }

#ifdef BLUMACH_COREMACH
    int runMachine(const BluMachCatalog &catalog, const QString &name, const QString &folder,
                   const Profile &profile, const QByteArray &firmware, const States &states)
    {
        Window window(catalog, name, folder, profile, firmware, states);
        const auto status = window.start();
        if (status != BM_STATUS_OK)
            return report("start_failed", QMessageBox::Warning, catalog.text(QStringLiteral("firmware.unavailable")),
                          catalog.text(QStringLiteral("coremach.start_failed")).arg(name, QString::number(static_cast<int>(status))),
                          startupFailureExitCode);

        // The VM manager drives this process as it drives an 86Box one.
        VMManagerClientSocket manager;
        const auto            socketPath = qgetenv("VMM_86BOX_SOCKET");
        if (!socketPath.isEmpty()) {
            manager.IPCConnect(QString::fromLocal8Bit(socketPath));
            QObject::connect(&manager, &VMManagerClientSocket::pause, &window, &Window::togglePause);
            QObject::connect(&manager, &VMManagerClientSocket::resetVM, &window, &Window::hardReset);
            QObject::connect(&manager, &VMManagerClientSocket::request_shutdown, &window, &QWidget::close);
            QObject::connect(&manager, &VMManagerClientSocket::force_shutdown, &window, &Window::forceShutdown);
            QObject::connect(&manager, &VMManagerClientSocket::showsettings, &window, &Window::showSettingsNotice);
            QObject::connect(&window, &Window::runningStateChanged, &manager, [&manager](bool paused) {
                manager.clientRunningStateChanged(paused ? VMManagerProtocol::RunningState::Paused
                                                         : VMManagerProtocol::RunningState::Running);
            });
            window.installEventFilter(&manager);
        }

#    ifdef Q_OS_WINDOWS
        // Sleep precisely enough for the session's short emulation slices.
        timeBeginPeriod(1);
#    endif
        window.show();
        window.adjustSize();
        if (!socketPath.isEmpty()) {
            manager.sendWinIdMessage(window.winId());
            manager.clientRunningStateChanged(VMManagerProtocol::RunningState::Running);
        }
        QApplication::exec();
#    ifdef Q_OS_WINDOWS
        timeEndPeriod(1);
#    endif
        return window.exitCode();
    }
#else
    int runMachine(const BluMachCatalog &, const QString &, const QString &, const Profile &, const QByteArray &,
                   const States &)
    {
        return startupFailureExitCode; // checkStartup() rejects every system first
    }
#endif
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
    for (const char *id : runnableSystems)
        if (systemId == QLatin1String(id))
            return knownSystem(systemId);
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
    const QString name     = machineName();
    const QString systemId = QString::fromUtf8(blumach_coremach_system);

    const auto check = checkStartup(systemId);
    if (check != StartupCheck::Ready)
        return report(startupCheckCode(check), QMessageBox::Warning, title,
                      startupMessage(catalog, check, name, systemId), startupFailureExitCode);

    if (settings_only)
        return report("settings_unavailable", QMessageBox::Information, name,
                      catalog.text(QStringLiteral("coremach.settings_unavailable")).arg(name), 0);

    const System *system = findSystem(systemId);
    Profile       profile;
    QString       badKey;
    QString       badValue;
    switch (readProfile(QString::fromUtf8(cfg_path), *system, &profile, &badKey, &badValue)) {
        case ProfileError::None:
            break;
        case ProfileError::InvalidOption:
            return report("invalid_option", QMessageBox::Warning, title,
                          catalog.text(QStringLiteral("coremach.invalid_option")).arg(name, badKey, badValue),
                          startupFailureExitCode);
        case ProfileError::Unreadable:
            return report("unreadable_configuration", QMessageBox::Warning, title,
                          catalog.text(QStringLiteral("coremach.start_failed")).arg(name, QStringLiteral("configuration")),
                          startupFailureExitCode);
    }

    const auto firmware = loadFirmware(system->firmware, romFolders());
    switch (firmware.status) {
        case FirmwareStatus::Ok:
            break;
        case FirmwareStatus::Missing:
        case FirmwareStatus::Unreadable:
            return report("firmware_missing", QMessageBox::Warning, title,
                          catalog.text(QStringLiteral("firmware.missing_body"))
                              .arg(name, QDir::toNativeSeparators(BluMachFirmware::directory()),
                                   QString::fromLatin1(system->firmware.path)),
                          startupFailureExitCode);
        case FirmwareStatus::WrongSize:
        case FirmwareStatus::WrongHash:
            return report("firmware_mismatch", QMessageBox::Warning, title,
                          catalog.text(QStringLiteral("coremach.firmware_mismatch"))
                              .arg(name, firmware.path, QString::number(system->firmware.size),
                                   QString::fromLatin1(system->firmware.sha256), QString::number(firmware.size),
                                   firmware.sha256),
                          startupFailureExitCode);
    }

    States        states;
    const QString folder = QString::fromUtf8(usr_path);
    for (size_t index = 0; index < system->stateCount; ++index) {
        const auto   &state   = system->states[index];
        const QString path    = stateFile(folder, state.role);
        bool          ignored = false;
        const auto    data    = loadState(path, state.size, &ignored);
        if (ignored)
            std::fprintf(stderr, "BluMach: CoreMach: ignoring %s, which is not %lld bytes\n",
                         QDir::toNativeSeparators(path).toUtf8().constData(), static_cast<long long>(state.size));
        if (!data.isEmpty())
            states.insert(QString::fromLatin1(state.role), data);
    }

    return runMachine(catalog, name, folder, profile, firmware.data, states);
}

}
