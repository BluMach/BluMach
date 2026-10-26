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
#include <QDateTime>
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
        { "olivetti-m10", "Olivetti M10", "Intel/OKI 80C85", "LCD 240 × 64 (HD44102)",
          true, // contrast knob on the right side panel (operations guide p. 1-10)
          true, // RS-232C through the IM6402
          { "machines/olivetti_m10/m10-system-first32k.bin", 32768,
            "43f85e3ac5cc264be3ad4c3b36dea82381409a66381146ce06a78c22704b0dbc" },
          m10Options, std::size(m10Options), m10States, std::size(m10States) },
    };

    const QString sectionName = QStringLiteral("CoreMach");
    const QString systemKey   = QStringLiteral("system");
    const QString displayKeyPrefix = QStringLiteral("lcd_");
    const QString soundKey         = QStringLiteral("sound");
    constexpr int contrastSteps = 5;
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
        if (key == systemKey || key == soundKey || key.startsWith(displayKeyPrefix))
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

EventLog::EventLog(const QString &machineFolder)
    : m_path(QDir(machineFolder).filePath(QStringLiteral("coremach-events.log")))
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    while (!file.atEnd()) {
        const QStringList fields = QString::fromUtf8(file.readLine()).trimmed().split(QLatin1Char('\t'));
        if (fields.size() >= 3)
            m_seen.insert(fields.at(1) + QLatin1Char('\t') + fields.at(2));
    }
}

bool
EventLog::record(const QString &kind, const QString &detail, const QString &context)
{
    const QString key = kind + QLatin1Char('\t') + detail;
    if (m_seen.contains(key))
        return false;
    m_seen.insert(key);
    if (m_seen.size() > maximumEntries)
        return true; // still new, but the file stops growing
    QFile file(m_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        const QString line = QStringList { QDateTime::currentDateTime().toString(Qt::ISODate), kind, detail, context }
                                 .join(QLatin1Char('\t'));
        file.write(line.toUtf8() + '\n');
    }
    return true;
}

DisplayPreferences
readDisplayPreferences(const QString &configPath, const System &system)
{
    DisplayPreferences preferences;
    const QSettings    settings(configPath, QSettings::IniFormat);
    const auto flag = [&settings](const char *key, bool fallback) {
        const QString value = settings.value(sectionName + QLatin1Char('/') + QLatin1String(key)).toString().trimmed();
        return value == QLatin1String("0") ? false : value == QLatin1String("1") ? true : fallback;
    };
    preferences.lcdEffect      = flag("lcd_effect", preferences.lcdEffect);
    preferences.lcdPersistence = flag("lcd_persistence", preferences.lcdPersistence);
    preferences.sound          = flag("sound", preferences.sound);
    if (system.contrastControl) {
        bool      number   = false;
        const int contrast = settings.value(sectionName + QStringLiteral("/lcd_contrast")).toString().toInt(&number);
        if (number && contrast >= 0 && contrast < contrastSteps)
            preferences.lcdContrast = contrast;
    }
    return preferences;
}

bool
writeCoreMachValue(const QString &configPath, const QString &key, const QString &value)
{
    QFile input(configPath);
    if (!input.open(QIODevice::ReadOnly))
        return false;
    const QByteArray original = input.readAll();
    input.close();
    const bool        crlf  = original.contains("\r\n");
    QList<QByteArray> lines = original.split('\n');
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    for (auto &line : lines)
        if (line.endsWith('\r'))
            line.chop(1);

    const QByteArray header = "[" + sectionName.toUtf8() + "]";
    const QByteArray entry  = key.toUtf8() + " = " + value.toUtf8();
    int  section = -1;
    int  end     = lines.size();
    bool written = false;
    for (int index = 0; index < lines.size(); ++index) {
        const QByteArray text = lines[index].trimmed();
        if (text.startsWith('[')) {
            if (section >= 0) {
                end = index;
                break;
            }
            if (text == header)
                section = index;
            continue;
        }
        if (section < 0 || text.startsWith('#') || text.startsWith(';'))
            continue;
        const int     equals = text.indexOf('=');
        const QString name   = QString::fromUtf8((equals < 0 ? text : text.left(equals)).trimmed());
        if (name == key) {
            lines[index] = entry;
            written      = true;
            break;
        }
    }
    if (!written) {
        if (section < 0)
            return false; // not a CoreMach machine
        // After the last non-blank line of the section.
        int insert = end;
        while (insert > section + 1 && lines[insert - 1].trimmed().isEmpty())
            --insert;
        lines.insert(insert, entry);
    }

    QByteArray output = lines.join(crlf ? "\r\n" : "\n");
    output += crlf ? "\r\n" : "\n";
    QSaveFile file(configPath);
    return file.open(QIODevice::WriteOnly) && file.write(output) == output.size() && file.commit();
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
