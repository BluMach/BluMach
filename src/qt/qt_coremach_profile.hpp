/*
 * BluMach bridge to the CoreMach engine: systems, machine profiles,
 * firmware and persistent state files.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_PROFILE_HPP
#define QT_COREMACH_PROFILE_HPP

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>

namespace CoreMachBridge {

using States = QHash<QString, QByteArray>; // persistent state by role

struct Option {
    const char     *key;
    uint32_t        defaultValue;
    const uint32_t *accepted;
    size_t          acceptedCount;
};

struct Firmware {
    const char *path; // relative to a ROM folder
    qint64      size;
    const char *sha256;
};

struct PersistentState {
    const char *role;
    qint64      size;
};

struct System {
    const char            *id;
    const char            *name;
    const char            *cpu;
    const char            *display;
    bool                   contrastControl; // the machine has a contrast knob
    bool                   serialPort;      // CoreMach models its serial port
    bool                   cassette;        // and its cassette interface
    bool                   printer;         // and its printer port
    Firmware               firmware;
    const Option          *options;
    size_t                 optionCount;
    const PersistentState *states;
    size_t                 stateCount;

    const Option *option(const QString &key) const;
};

// Every CoreMach system known to BluMach, whether or not this build runs it.
const System *knownSystem(const QString &systemId);

// The [CoreMach] settings of a machine, validated against its system.
struct Profile {
    const System             *system = nullptr;
    QHash<QString, uint32_t> options;

    uint32_t value(const char *key) const;
};

enum class ProfileError {
    None,
    Unreadable,
    InvalidOption,
};

// Reads the [CoreMach] section of a configuration file without changing it.
// Absent options take their defaults; on InvalidOption, badKey and badValue
// name the first rejected setting.
ProfileError readProfile(const QString &configPath, const System &system, Profile *profile,
                         QString *badKey = nullptr, QString *badValue = nullptr);

enum class FirmwareStatus {
    Ok,
    Missing,
    Unreadable,
    WrongSize,
    WrongHash,
};

struct FirmwareImage {
    FirmwareStatus status = FirmwareStatus::Missing;
    QString        path;   // first matching file, empty when missing
    QByteArray     data;   // only when status is Ok
    qint64         size = 0;
    QString        sha256; // lowercase hex of the file read
};

// Looks for the firmware in each ROM folder in order and verifies the first
// file found against the expected size and SHA-256.
FirmwareImage loadFirmware(const Firmware &firmware, const QStringList &romFolders);

// Events of a CoreMach machine that BluMach tolerated, kept once each in
// <machine folder>/coremach-events.log, also across sessions. Each line is
// "<local time>\t<kind>\t<detail>\t<context>".
class EventLog final {
public:
    static constexpr int maximumEntries = 1000;

    explicit EventLog(const QString &machineFolder);
    // Appends the event unless kind and detail were already recorded.
    // Returns true when the event is new in this machine folder.
    bool    record(const QString &kind, const QString &detail, const QString &context = QString());
    QString path() const { return m_path; }

private:
    QString       m_path;
    QSet<QString> m_seen;
};

// Display, sound and tolerance preferences of a machine, kept as lcd_*
// keys, "sound" and "tolerant" in its [CoreMach] section. They are not
// machine options: readProfile() skips them.
struct DisplayPreferences {
    bool lcdEffect      = true;
    bool lcdPersistence = true;
    int  lcdContrast    = 2;
    bool sound          = true;
    bool tolerant       = true; // 0: stop on the first unsupported operation
};
DisplayPreferences readDisplayPreferences(const QString &configPath, const System &system);
// Sets one key of the [CoreMach] section, keeping every other line of the
// file as it is, and replaces the file atomically.
bool writeCoreMachValue(const QString &configPath, const QString &key, const QString &value);

// <machine folder>/state/<role>.bin
QString stateFile(const QString &machineFolder, const char *role);
// Saved state of the expected size; empty when absent or of another size.
QByteArray loadState(const QString &path, qint64 size, bool *ignored = nullptr);
// Atomically replaces the state file, creating its folder when needed.
bool saveState(const QString &path, const QByteArray &data);

}

#endif
