// SPDX-License-Identifier: GPL-2.0-or-later
// CoreMach bridge contracts: profiles, firmware, persistent states, catalogue
// consistency and, when CoreMach is built, a session on a synthetic ROM.
// No firmware or guest media is needed.
#include "qt_coremach_profile.hpp"
#ifdef BLUMACH_COREMACH
#    include "qt_coremach_keys.hpp"
#    include "qt_coremach_session.hpp"
#endif

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <mutex>

using namespace CoreMachBridge;

namespace {
QString write(const QDir &dir, const QString &name, const QByteArray &data)
{
    const QString path = dir.filePath(name);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
        return QString();
    return path;
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

const System &m10()
{
    const System *system = knownSystem(QStringLiteral("olivetti-m10"));
    Q_ASSERT(system);
    return *system;
}
}

class CoreMachBridgeTest : public QObject {
    Q_OBJECT
private slots:
    void systemTable()
    {
        QVERIFY(!knownSystem(QStringLiteral("olivetti-m99")));
        const System &system = m10();
        QCOMPARE(system.firmware.size, 32768);
        QCOMPARE(QString::fromLatin1(system.firmware.sha256),
                 QStringLiteral("43f85e3ac5cc264be3ad4c3b36dea82381409a66381146ce06a78c22704b0dbc"));
        for (size_t index = 0; index < system.optionCount; ++index) {
            const Option &option = system.options[index];
            QVERIFY(std::find(option.accepted, option.accepted + option.acceptedCount, option.defaultValue)
                    != option.accepted + option.acceptedCount);
        }
        QVERIFY(!system.option(QStringLiteral("system")));
    }

    void profileDefaultsAndValues()
    {
        QTemporaryDir dir;
        const QByteArray config = "[CoreMach]\nsystem = olivetti-m10\nram_kib = 8\nlanguage = 3\n";
        const QString    path   = write(QDir(dir.path()), QStringLiteral("blumach.cfg"), config);
        Profile          profile;
        QCOMPARE(readProfile(path, m10(), &profile), ProfileError::None);
        QCOMPARE(profile.system, &m10());
        QCOMPARE(profile.value("ram_kib"), 8u);
        QCOMPARE(profile.value("language"), 3u);
        QCOMPARE(profile.value("rtc_time"), 0u); // default
        QCOMPARE(read(path), config);
    }

    void profileRejectsUnsupportedSettings_data()
    {
        QTest::addColumn<QByteArray>("setting");
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("value");
        QTest::newRow("value") << QByteArray("ram_kib = 64") << "ram_kib" << "64";
        QTest::newRow("text") << QByteArray("language = it") << "language" << "it";
        QTest::newRow("unknown") << QByteArray("mem_size = 32") << "mem_size" << "32";
    }

    void profileRejectsUnsupportedSettings()
    {
        QFETCH(QByteArray, setting);
        QFETCH(QString, key);
        QFETCH(QString, value);
        QTemporaryDir dir;
        const QString path = write(QDir(dir.path()), QStringLiteral("blumach.cfg"),
                                   "[CoreMach]\nsystem = olivetti-m10\n" + setting + "\n");
        Profile profile;
        QString badKey;
        QString badValue;
        QCOMPARE(readProfile(path, m10(), &profile, &badKey, &badValue), ProfileError::InvalidOption);
        QCOMPARE(badKey, key);
        QCOMPARE(badValue, value);
        QCOMPARE(readProfile(dir.filePath(QStringLiteral("missing.cfg")), m10(), &profile),
                 ProfileError::Unreadable);
    }

    void firmwareIsVerified()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        const QByteArray image(32768, '\x5a');
        const QByteArray hash = QCryptographicHash::hash(image, QCryptographicHash::Sha256).toHex();
        const Firmware   firmware { "machines/test/firmware.bin", 32768, hash.constData() };
        const QStringList folders { first.path(), second.path() };

        QCOMPARE(loadFirmware(firmware, folders).status, FirmwareStatus::Missing);

        write(QDir(second.path()), QStringLiteral("machines/test/firmware.bin"), image);
        auto loaded = loadFirmware(firmware, folders);
        QCOMPARE(loaded.status, FirmwareStatus::Ok);
        QCOMPARE(loaded.data, image);
        QCOMPARE(loaded.sha256, QString::fromLatin1(hash));

        // The first folder holding the file decides; it is never skipped.
        write(QDir(first.path()), QStringLiteral("machines/test/firmware.bin"), image.left(16384));
        loaded = loadFirmware(firmware, folders);
        QCOMPARE(loaded.status, FirmwareStatus::WrongSize);
        QCOMPARE(loaded.size, 16384);
        QVERIFY(loaded.data.isEmpty());

        QByteArray changed = image;
        changed[100]       = '\0';
        write(QDir(first.path()), QStringLiteral("machines/test/firmware.bin"), changed);
        loaded = loadFirmware(firmware, folders);
        QCOMPARE(loaded.status, FirmwareStatus::WrongHash);
        QVERIFY(loaded.data.isEmpty());
        QVERIFY(loaded.sha256 != QString::fromLatin1(hash));

        QCOMPARE(loadFirmware(m10().firmware, { second.path() }).status, FirmwareStatus::Missing);
    }

    void persistentStates()
    {
        QTemporaryDir dir;
        const QString path = stateFile(dir.path(), "ram");
        QCOMPARE(QDir::cleanPath(path), QDir::cleanPath(dir.filePath(QStringLiteral("state/ram.bin"))));
        bool ignored = true;
        QVERIFY(loadState(path, 32768, &ignored).isEmpty());
        QVERIFY(!ignored);

        QByteArray data(32768, '\0');
        for (int index = 0; index < data.size(); ++index)
            data[index] = static_cast<char>(index * 7);
        QVERIFY(saveState(path, data));
        QCOMPARE(loadState(path, 32768, &ignored), data);
        QVERIFY(!ignored);
        QCOMPARE(QDir(dir.filePath(QStringLiteral("state"))).entryList(QDir::Files), QStringList { "ram.bin" });

        QVERIFY(loadState(path, 40, &ignored).isEmpty());
        QVERIFY(ignored);
    }

    void catalogueMatchesSystems()
    {
        QFile file(QStringLiteral(BLUMACH_TEST_CATALOG_JSON));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
        const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
        QHash<QString, QString> systemByPlatform;
        for (const auto &value : catalog.value(QStringLiteral("platforms")).toArray()) {
            const auto platform = value.toObject();
            if (platform.value(QStringLiteral("engine")).toString() != QStringLiteral("coremach"))
                continue;
            const auto id = platform.value(QStringLiteral("coremach_system_id")).toString();
            QVERIFY2(knownSystem(id), qPrintable(id));
            systemByPlatform.insert(platform.value(QStringLiteral("id")).toString(), id);
        }
        for (const auto &value : catalog.value(QStringLiteral("products")).toArray()) {
            const auto product = value.toObject();
            const auto id      = product.value(QStringLiteral("id")).toString();
            if (!systemByPlatform.contains(product.value(QStringLiteral("platform_id")).toString()))
                continue;
            const System &system = *knownSystem(systemByPlatform.value(product.value(QStringLiteral("platform_id")).toString()));
            const auto files = product.value(QStringLiteral("firmware")).toObject().value(QStringLiteral("required_files")).toArray();
            QCOMPARE(files.size(), 1);
            QCOMPARE(files.at(0).toString(), QString::fromLatin1(system.firmware.path));

            const auto check = [&system, &id](const QString &key, const QJsonValue &setting) {
                const Option *option = system.option(key);
                QVERIFY2(option, qPrintable(id + QLatin1Char(' ') + key));
                const auto number = static_cast<uint32_t>(setting.toInt(-1));
                QVERIFY2(std::find(option->accepted, option->accepted + option->acceptedCount, number)
                             != option->accepted + option->acceptedCount,
                         qPrintable(id + QLatin1Char(' ') + key));
            };
            const auto creation = product.value(QStringLiteral("creation")).toObject();
            for (const auto &section : creation.value(QStringLiteral("configuration")).toArray()) {
                const auto values = section.toObject().value(QStringLiteral("values")).toObject();
                for (auto it = values.constBegin(); it != values.constEnd(); ++it)
                    check(it.key(), it.value());
            }
            for (const auto &field : creation.value(QStringLiteral("fields")).toArray())
                for (const auto &choice : field.toObject().value(QStringLiteral("choices")).toArray())
                    for (const auto &setting : choice.toObject().value(QStringLiteral("set")).toArray())
                        check(setting.toObject().value(QStringLiteral("key")).toString(),
                              setting.toObject().value(QStringLiteral("value")));
        }
    }

#ifdef BLUMACH_COREMACH
    void keysFollowPhysicalPositions()
    {
        QCOMPARE(keyFromSetOneScanCode(0x01e), BM_KEY_A);
        QCOMPARE(keyFromSetOneScanCode(0x01d), BM_KEY_LEFT_CONTROL);
        QCOMPARE(keyFromSetOneScanCode(0x11d), BM_KEY_RIGHT_CONTROL);
        QCOMPARE(keyFromSetOneScanCode(0x148), BM_KEY_UP);
        QCOMPARE(keyFromSetOneScanCode(0x047), noKey); // keypad 7: no M10 key
        // Linux input codes share the set 1 main block; extended keys differ.
        QCOMPARE(keyFromEvdevCode(30), BM_KEY_A);
        QCOMPARE(keyFromEvdevCode(97), BM_KEY_RIGHT_CONTROL);
        QCOMPARE(keyFromEvdevCode(105), BM_KEY_LEFT);
        QCOMPARE(keyFromEvdevCode(111), BM_KEY_DELETE);
        QCOMPARE(keyFromEvdevCode(200), noKey);
        // Layout-dependent fallback.
        QCOMPARE(keyFromQtKey(Qt::Key_Q, 0), BM_KEY_Q);
        QCOMPARE(keyFromQtKey(Qt::Key_F5, 0), BM_KEY_F5);
        QCOMPARE(keyFromQtKey(Qt::Key_0, 0), BM_KEY_0);
        QCOMPARE(keyFromQtKey(Qt::Key_Shift, 0xa1), BM_KEY_RIGHT_SHIFT);
        QCOMPARE(keyFromQtKey(Qt::Key_Shift, 0), BM_KEY_LEFT_SHIFT);
        QCOMPARE(keyFromQtKey(Qt::Key_F11, 0), noKey);
    }

    void sessionRunsSyntheticRom()
    {
        // JMP 0000h: the CPU loops in ROM and never writes RAM.
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3';
        QByteArray ram(32768, '\0');
        for (int index = 0; index < ram.size(); ++index)
            ram[index] = static_cast<char>((index * 13) ^ 0x5a);

        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        std::mutex mutex;
        QImage     lastFrame;
        std::atomic<int> frames { 0 };
        Session session(profile, rom, { { QStringLiteral("ram"), ram } }, [&](const Session::Frame &frame) {
            if (frame.image.isNull())
                return;
            std::lock_guard<std::mutex> lock(mutex);
            lastFrame = frame.image;
            ++frames;
        });
        QCOMPARE(session.start(), BM_STATUS_OK);
        QTRY_VERIFY_WITH_TIMEOUT(frames.load() >= 3, 5000);
        {
            std::lock_guard<std::mutex> lock(mutex);
            QCOMPARE(lastFrame.size(), QSize(240, 64));
        }
        QCOMPARE(session.state(), BM_SESSION_RUNNING);
        const uint64_t started = session.ticks();
        QTRY_VERIFY_WITH_TIMEOUT(session.ticks() > started, 2000);

        session.pause();
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), BM_SESSION_PAUSED, 2000);
        const uint64_t paused = session.ticks();
        QTest::qWait(100);
        QCOMPARE(session.ticks(), paused);
        session.resume();
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), BM_SESSION_RUNNING, 2000);

        session.key(BM_KEY_A, true);
        session.key(BM_KEY_A, false);
        session.reset();
        QTRY_VERIFY_WITH_TIMEOUT(frames.load() >= 6, 5000);

        const States captured = session.capture();
        QCOMPARE(captured.value(QStringLiteral("ram")).size(), 32768);
        QCOMPARE(captured.value(QStringLiteral("rtc")).size(), 40);
        QCOMPARE(captured.value(QStringLiteral("ram")), ram);

        const States final = session.stop();
        QCOMPARE(session.state(), BM_SESSION_STOPPED);
        QCOMPARE(final.value(QStringLiteral("ram")), ram);
        QCOMPARE(final.value(QStringLiteral("rtc")).size(), 40);
        QCOMPARE(session.capture(), final);
    }

    // Opt-in: BLUMACH_TEST_ROM_DIR names a ROM folder holding the M10
    // firmware. BLUMACH_TEST_OUTPUT_DIR, if set, receives the last frame.
    void realM10Firmware()
    {
        const QString romDir = qEnvironmentVariable("BLUMACH_TEST_ROM_DIR");
        if (romDir.isEmpty())
            QSKIP("BLUMACH_TEST_ROM_DIR is not set");
        const auto firmware = loadFirmware(m10().firmware, { romDir });
        QCOMPARE(firmware.status, FirmwareStatus::Ok);

        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);
        std::mutex mutex;
        QImage     lastFrame;
        Session    session(profile, firmware.data, {}, [&](const Session::Frame &frame) {
            if (frame.image.isNull())
                return;
            std::lock_guard<std::mutex> lock(mutex);
            lastFrame = frame.image;
        });
        QCOMPARE(session.start(), BM_STATUS_OK);
        // Three seconds of guest time reach the firmware's main menu.
        QTRY_VERIFY_WITH_TIMEOUT(session.ticks() >= UINT64_C(3000000000), 20000);
        QTest::qWait(100);
        QImage frame;
        {
            std::lock_guard<std::mutex> lock(mutex);
            frame = lastFrame;
        }
        QCOMPARE(frame.size(), QSize(240, 64));
        QSet<QRgb> colours;
        for (int y = 0; y < frame.height(); ++y)
            for (int x = 0; x < frame.width(); ++x)
                colours.insert(frame.pixel(x, y));
        QVERIFY2(colours.size() >= 2, "the LCD shows nothing");
        const QString output = qEnvironmentVariable("BLUMACH_TEST_OUTPUT_DIR");
        if (!output.isEmpty())
            QVERIFY(frame.scaled(frame.size() * 3).save(QDir(output).filePath(QStringLiteral("m10-frame.png"))));

        const States states = session.stop();
        QCOMPARE(states.value(QStringLiteral("ram")).size(), 32768);
        QCOMPARE(states.value(QStringLiteral("rtc")).size(), 40);
    }

    void sessionRejectsInvalidFirmware()
    {
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        Session session(profile, QByteArray(1024, '\0'), {}, {});
        QVERIFY(session.start() != BM_STATUS_OK);
        QVERIFY(session.stop().isEmpty());
    }
#endif
};

QTEST_GUILESS_MAIN(CoreMachBridgeTest)
#include "coremach_bridge_test.moc"
