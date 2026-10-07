/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 Project BluMach.
 */
#include "qt_firmware.hpp"
#include "qt_vmmanager_config.hpp"
#include "qt_blumach_catalog.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <cstring>
extern "C" {
#include "../cpu/cpu.h"
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/machine.h>
#include <86box/video.h>
extern int rom_present(const char *);
extern int rom_set_user_path(const char *);
}
namespace BluMachFirmware {
static bool setPath(const QString &path)
{
    const auto bytes = QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toUtf8();
    if (bytes.size() >= static_cast<qsizetype>(sizeof(rom_path) - 1) || !rom_set_user_path(bytes.constData()))
        return false;
    std::memcpy(rom_path, bytes.constData(), static_cast<size_t>(bytes.size()) + 1);
    return true;
}
void restoreDirectory()
{
    if (rom_path[0])
        return; // Explicit --rompath wins at startup.
    VMManagerConfig config(VMManagerConfig::ConfigType::General);
    const auto saved = config.getStringValue(QStringLiteral("blumach_rom_directory"));
    if (!saved.isEmpty())
        setPath(saved); // Retain unavailable folders so the user can repair them.
}
QString directory()
{
    return rom_path[0] ? QString::fromUtf8(rom_path) : QCoreApplication::applicationDirPath() + QStringLiteral("/roms");
}
bool applyDirectory(const QString &path, QString *error)
{
    BluMachCatalog catalog;
    catalog.load();
    if (!QFileInfo(path).isDir() || !setPath(path)) {
        if (error)
            *error = catalog.text(QStringLiteral("firmware.invalid_directory"));
        return false;
    }
    VMManagerConfig config(VMManagerConfig::ConfigType::General);
    config.setStringValue(QStringLiteral("blumach_rom_directory"), QString::fromUtf8(rom_path));
    config.sync();
    return true;
}
int rescan()
{
    int count = 0;
    for (int id = 0; id < machine_count(); ++id)
        count += !!machine_available(id);
    return count;
}
QString startupError()
{
    BluMachCatalog catalog;
    catalog.load();
    if (machine_get_valid_ram(machine, static_cast<int>(mem_size)) != static_cast<int>(mem_size))
        return catalog.text(QStringLiteral("firmware.invalid_ram"));
    QStringList missing;
    const auto *device = machine_get_device(machine);
    if (device_configured_bios_available(device) < 0) {
        if (const auto *bios = device_configured_bios(device)) {
            for (int i = 0; i < bios->files_no; ++i) {
                if (!rom_present(bios->files[i])) {
                    QString file = QString::fromUtf8(bios->files[i]);
                    if (file.startsWith(QStringLiteral("roms/")))
                        file.remove(0, 5);
                    missing.append(file);
                }
            }
        } else
            missing.append(catalog.text(QStringLiteral("firmware.unknown_bios")));
    } else if (!machine_available(machine)) {
        for (const auto &product : catalog.products()) {
            const auto *platform = catalog.platform(product.platformId);
            if (!platform || platform->emulatorMachineId != QString::fromUtf8(machine_get_internal_name_ex(machine)))
                continue;
            for (const auto &entry : product.firmware.value(QStringLiteral("required_files")).toArray()) {
                const auto relative = entry.toString();
                if (!rom_present((QStringLiteral("roms/") + relative).toUtf8().constData()))
                    missing.append(relative);
            }
        }
        if (missing.isEmpty())
            missing.append(QString::fromUtf8(machine_getname(machine)));
    }
    for (int i = 0; i < GFXCARD_MAX; ++i)
        if (!video_card_available(gfxcard[i]))
            missing.append(QString::fromUtf8(video_get_internal_name(gfxcard[i])));
    if (missing.isEmpty())
        return {};
    return catalog.text(QStringLiteral("firmware.missing_body"))
        .arg(QString::fromUtf8(machine_getname(machine)), QDir::toNativeSeparators(directory()), missing.join('\n'));
}
}
