/*
 * BluMach bridge to the CoreMach engine: systems, machine profiles,
 * firmware and persistent state files.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_profile.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

#include <algorithm>
#include <iterator>

namespace CoreMachBridge {
namespace {
    // Olivetti M10: options and states of CoreMach's systems/olivetti-m10.
    // Keep the options in step with COREMACH_SYSTEMS in tools/catalog_audit.py.
    const uint32_t m10RamKib[]   = { 8, 16, 24, 32 };
    const uint32_t m10Language[] = { 0, 1, 2, 3 }; // English, German, Italian, French
    const uint32_t m10RtcTime[]  = { 0, 1 };       // host time, fixed reproducible start

    const Option m10Options[] = {
        { "ram_kib", 32, m10RamKib, std::size(m10RamKib) },
        { "language", 2, m10Language, std::size(m10Language) },
        { "rtc_time", 0, m10RtcTime, std::size(m10RtcTime) },
    };

    const PersistentState m10States[] = {
        { "ram", 32768 },
        { "rtc", 40 },
    };

    const System systems[] = {
        { "olivetti-m10", "Olivetti M10", "Intel/OKI 80C85",
          { "machines/olivetti_m10/m10-system-first32k.bin", 32768,
            "43f85e3ac5cc264be3ad4c3b36dea82381409a66381146ce06a78c22704b0dbc" },
          m10Options, std::size(m10Options), m10States, std::size(m10States) },
    };

    const QString sectionName = QStringLiteral("CoreMach");
    const QString systemKey   = QStringLiteral("system");
}

const Option *
System::option(const QString &key) const
{
    for (size_t index = 0; index < optionCount; ++index)
        if (key == QLatin1String(options[index].key))
            return &options[index];
    return nullptr;
}

const System *
knownSystem(const QString &systemId)
{
    for (const auto &system : systems)
        if (systemId == QLatin1String(system.id))
            return &system;
    return nullptr;
}

uint32_t
Profile::value(const char *key) const
{
    return options.value(QString::fromLatin1(key));
}

ProfileError
readProfile(const QString &configPath, const System &system, Profile *profile,
            QString *badKey, QString *badValue)
{
    if (!QFileInfo(configPath).isReadable())
        return ProfileError::Unreadable;
    const QSettings settings(configPath, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError)
        return ProfileError::Unreadable;

    const auto reject = [badKey, badValue](const QString &key, const QString &value) {
        if (badKey)
            *badKey = key;
        if (badValue)
            *badValue = value;
        return ProfileError::InvalidOption;
    };

    Profile result;
    result.system = &system;
    for (size_t index = 0; index < system.optionCount; ++index)
        result.options.insert(QString::fromLatin1(system.options[index].key), system.options[index].defaultValue);

    const QString prefix = sectionName + QLatin1Char('/');
    for (const auto &fullKey : settings.allKeys()) {
        if (!fullKey.startsWith(prefix))
            continue;
        const QString key   = fullKey.mid(prefix.size());
        const QString value = settings.value(fullKey).toString().trimmed();
        if (key == systemKey)
            continue;
        const Option *option = system.option(key);
        if (!option)
            return reject(key, value);
        bool     isNumber = false;
        const uint32_t number = value.toUInt(&isNumber, 10);
        if (!isNumber || std::find(option->accepted, option->accepted + option->acceptedCount, number)
                             == option->accepted + option->acceptedCount)
            return reject(key, value);
        result.options.insert(key, number);
    }
    *profile = result;
    return ProfileError::None;
}

FirmwareImage
loadFirmware(const Firmware &firmware, const QStringList &romFolders)
{
    FirmwareImage image;
    for (const auto &folder : romFolders) {
        const QString candidate = QDir(folder).filePath(QString::fromLatin1(firmware.path));
        if (QFileInfo(candidate).isFile()) {
            image.path = QDir::toNativeSeparators(QDir::cleanPath(candidate));
            break;
        }
    }
    if (image.path.isEmpty())
        return image;

    QFile file(image.path);
    if (!file.open(QIODevice::ReadOnly)) {
        image.status = FirmwareStatus::Unreadable;
        return image;
    }
    // Never read more than one byte past the expected size.
    QByteArray data = file.read(firmware.size + 1);
    image.size      = file.size();
    image.sha256    = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    if (image.size != firmware.size || data.size() != firmware.size) {
        image.status = FirmwareStatus::WrongSize;
        return image;
    }
    if (image.sha256 != QLatin1String(firmware.sha256)) {
        image.status = FirmwareStatus::WrongHash;
        return image;
    }
    image.status = FirmwareStatus::Ok;
    image.data   = data;
    return image;
}

QString
stateFile(const QString &machineFolder, const char *role)
{
    return QDir(machineFolder).filePath(QStringLiteral("state/%1.bin").arg(QString::fromLatin1(role)));
}

QByteArray
loadState(const QString &path, qint64 size, bool *ignored)
{
    if (ignored)
        *ignored = false;
    QFile file(path);
    if (!file.exists())
        return {};
    if (file.size() != size || !file.open(QIODevice::ReadOnly)) {
        if (ignored)
            *ignored = true;
        return {};
    }
    QByteArray data = file.readAll();
    if (data.size() != size) {
        if (ignored)
            *ignored = true;
        return {};
    }
    return data;
}

bool
saveState(const QString &path, const QByteArray &data)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile output(path);
    return output.open(QIODevice::WriteOnly) && output.write(data) == data.size() && output.commit();
}

}
