// SPDX-License-Identifier: GPL-2.0-or-later
// CoreMach bridge contracts: profiles, firmware, persistent states, catalogue
// consistency and, when CoreMach is built, a session on a synthetic ROM.
// No firmware or guest media is needed.
#include "qt_coremach_profile.hpp"
#ifdef BLUMACH_COREMACH
#    include "qt_coremach_buzzer.hpp"
#    include "qt_coremach_display.hpp"
#    include "qt_coremach_keys.hpp"
#    include "qt_coremach_lcd.hpp"
#    include "qt_coremach_session.hpp"
#    include "qt_coremach_tape.hpp"
#endif

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

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
        QVERIFY(system.printer);
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

    void eventLogRecordsEachEventOnce()
    {
        QTemporaryDir dir;
        {
            EventLog log(dir.path());
            QCOMPARE(QFileInfo(log.path()).fileName(), QStringLiteral("coremach-events.log"));
            QVERIFY(log.record(QStringLiteral("key_without_position"), QStringLiteral("ESC"), QStringLiteral("Olivetti M10")));
            QVERIFY(!log.record(QStringLiteral("key_without_position"), QStringLiteral("ESC"), QStringLiteral("again")));
            QVERIFY(log.record(QStringLiteral("key_without_position"), QStringLiteral("Home")));
        }
        // A later session keeps the earlier events.
        EventLog later(dir.path());
        QVERIFY(!later.record(QStringLiteral("key_without_position"), QStringLiteral("ESC")));
        QVERIFY(later.record(QStringLiteral("video_failed"), QStringLiteral("status -6")));
        const auto lines = QString::fromUtf8(read(later.path())).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        const auto first = lines.at(0).split(QLatin1Char('\t'));
        QCOMPARE(first.size(), 4);
        QVERIFY(QDateTime::fromString(first.at(0), Qt::ISODate).isValid());
        QCOMPARE(first.mid(1), (QStringList { "key_without_position", "ESC", "Olivetti M10" }));
    }

    void displayPreferencesInTheConfiguration()
    {
        QTemporaryDir dir;
        // Comments, other sections and spacing must survive.
        const QByteArray config = "; machine\r\n[CoreMach]\r\nsystem = olivetti-m10\r\nram_kib = 32\r\n"
                                  "lcd_effect = 1\r\n\r\n[Other]\r\nkeep = yes\r\n";
        const QString path = write(QDir(dir.path()), QStringLiteral("blumach.cfg"), config);

        auto preferences = readDisplayPreferences(path, m10());
        QVERIFY(preferences.lcdEffect && preferences.lcdPersistence);
        QCOMPARE(preferences.lcdContrast, 2);

        QVERIFY(writeCoreMachValue(path, QStringLiteral("lcd_effect"), QStringLiteral("0")));
        QVERIFY(writeCoreMachValue(path, QStringLiteral("lcd_contrast"), QStringLiteral("4")));
        QCOMPARE(read(path), QByteArray("; machine\r\n[CoreMach]\r\nsystem = olivetti-m10\r\nram_kib = 32\r\n"
                                        "lcd_effect = 0\r\nlcd_contrast = 4\r\n\r\n[Other]\r\nkeep = yes\r\n"));
        preferences = readDisplayPreferences(path, m10());
        QVERIFY(!preferences.lcdEffect);
        QCOMPARE(preferences.lcdContrast, 4);

        // Sound and tolerance are on unless the machine says otherwise.
        QVERIFY(preferences.sound && preferences.tolerant);
        QVERIFY(writeCoreMachValue(path, QStringLiteral("tolerant"), QStringLiteral("0")));
        QVERIFY(!readDisplayPreferences(path, m10()).tolerant);
        QVERIFY(writeCoreMachValue(path, QStringLiteral("sound"), QStringLiteral("0")));
        QVERIFY(!readDisplayPreferences(path, m10()).sound);

        // lcd_* and sound are preferences, not machine options.
        Profile profile;
        QCOMPARE(readProfile(path, m10(), &profile), ProfileError::None);
        QCOMPARE(profile.value("ram_kib"), 32u);

        // Out-of-range or garbled values keep the defaults.
        QVERIFY(writeCoreMachValue(path, QStringLiteral("lcd_contrast"), QStringLiteral("9")));
        QVERIFY(writeCoreMachValue(path, QStringLiteral("lcd_persistence"), QStringLiteral("maybe")));
        preferences = readDisplayPreferences(path, m10());
        QCOMPARE(preferences.lcdContrast, 2);
        QVERIFY(preferences.lcdPersistence);

        // Never on a file without a [CoreMach] section.
        const QByteArray other = "[Machine]\nmachine = ibmxt\n";
        const QString    pc    = write(QDir(dir.path()), QStringLiteral("pc.cfg"), other);
        QVERIFY(!writeCoreMachValue(pc, QStringLiteral("lcd_effect"), QStringLiteral("0")));
        QCOMPARE(read(pc), other);
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

    void displayKeepsAltAndMnemonics()
    {
        // The Windows style activates the menu bar on Alt, as on Windows.
        QMainWindow window;
        window.setStyle(QStyleFactory::create(QStringLiteral("Windows")));
        auto *menu    = window.menuBar()->addMenu(QStringLiteral("&Action"));
        menu->addAction(QStringLiteral("&Pause"));
        auto *central = new QWidget(&window);
        auto *edit    = new QLineEdit(central);
        auto *display = new CoreMachBridge::Display(central);
        display->move(0, 40);
        window.setCentralWidget(central);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        // Control: another widget lets Alt reach the menu bar.
        edit->setFocus();
        QTRY_VERIFY(edit->hasFocus());
        QTest::keyClick(edit, Qt::Key_Alt);
        const bool altReachesMenus = window.menuBar()->activeAction() != nullptr;
        QTest::keyClick(window.menuBar(), Qt::Key_Escape);
        QTest::keyClick(edit, Qt::Key_Escape);

        display->setFocus();
        QTRY_VERIFY(display->hasFocus());
        QSignalSpy keys(display, &CoreMachBridge::Display::key);
        QTest::keyPress(display, Qt::Key_Alt);
        QTest::keyRelease(display, Qt::Key_Alt);
        QCOMPARE(window.menuBar()->activeAction(), nullptr);
        QVERIFY(display->hasFocus());
        QCOMPARE(keys.count(), 2);
        QCOMPARE(keys.at(0).at(0).value<bm_key_code_t>(), BM_KEY_LEFT_ALT);
        QCOMPARE(keys.at(0).at(1).toBool(), true);
        QCOMPARE(keys.at(1).at(1).toBool(), false);

        // GRPH+A types on the machine instead of opening "&Action".
        keys.clear();
        QTest::keyClick(display, Qt::Key_A, Qt::AltModifier);
        QVERIFY(!menu->isVisible());
        QVERIFY(display->hasFocus());
        // Alt down, A down, A up, Alt up.
        QCOMPARE(keys.count(), 4);
        QCOMPARE(keys.at(0).at(0).value<bm_key_code_t>(), BM_KEY_LEFT_ALT);
        QCOMPARE(keys.at(1).at(0).value<bm_key_code_t>(), BM_KEY_A);
        QCOMPARE(keys.at(1).at(1).toBool(), true);
        QCOMPARE(keys.at(3).at(0).value<bm_key_code_t>(), BM_KEY_LEFT_ALT);
        QCOMPARE(keys.at(3).at(1).toBool(), false);
        if (!altReachesMenus)
            qInfo("note: this platform style does not activate menus on Alt");
    }

    void keyNamesForNotices()
    {
        QCOMPARE(keyName(BM_KEY_ESCAPE), QStringLiteral("ESC"));
        QCOMPARE(keyName(BM_KEY_HOME), QStringLiteral("Home"));
        QCOMPARE(keyName(BM_KEY_F5), QStringLiteral("F5"));
        QCOMPARE(keyName(BM_KEY_Q), QStringLiteral("Q"));
        QCOMPARE(keyName(BM_KEY_0), QStringLiteral("0"));
    }

    void keysWithoutPositionDoNotStopTheSession()
    {
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3'; // JMP 0000h
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        std::mutex                     mutex;
        QList<Session::Notice>         notices;
        std::atomic<int>               failures { 0 };
        Session session(profile, rom, {}, [&](const Session::Frame &frame) {
            if (frame.status != BM_STATUS_OK)
                ++failures;
            if (frame.notice.kind == Session::Notice::Kind::None)
                return;
            std::lock_guard<std::mutex> lock(mutex);
            notices.append(frame.notice);
        });
        QCOMPARE(session.start(), BM_STATUS_OK);
        // The M10 has no ESC or Home key; A is on its keyboard.
        session.key(BM_KEY_ESCAPE, true);
        session.key(BM_KEY_ESCAPE, false);
        session.key(BM_KEY_HOME, true);
        session.key(BM_KEY_A, true);
        session.key(BM_KEY_A, false);
        QTRY_VERIFY_WITH_TIMEOUT([&] { std::lock_guard<std::mutex> lock(mutex); return notices.size() >= 3; }(), 2000);
        const uint64_t ticks = session.ticks();
        QTRY_VERIFY_WITH_TIMEOUT(session.ticks() > ticks, 2000);
        QCOMPARE(session.state(), BM_SESSION_RUNNING);
        QCOMPARE(failures.load(), 0);
        {
            std::lock_guard<std::mutex> lock(mutex);
            QCOMPARE(notices.size(), 3);
            for (const auto &notice : notices) {
                QCOMPARE(notice.kind, Session::Notice::Kind::KeyWithoutPosition);
                QCOMPARE(notice.status, BM_STATUS_UNSUPPORTED);
            }
            QCOMPARE(notices.at(0).key, BM_KEY_ESCAPE);
            QCOMPARE(notices.at(2).key, BM_KEY_HOME);
        }
        session.stop();
    }

    void failedRunStopsTheSession()
    {
        // DSUB (08h) is undocumented; CoreMach rejects it in strict mode
        // (tolerant = 0), so the run fails and the session must stop with
        // that status.
        QByteArray rom(32768, '\0');
        rom[0] = '\x08';
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("rtc_time"), 1);
        std::atomic<int> failure { BM_STATUS_OK };
        Session session(profile, rom, {}, [&](const Session::Frame &frame) {
            if (frame.status != BM_STATUS_OK)
                failure = frame.status;
        }, false);
        QCOMPARE(session.start(), BM_STATUS_OK);
        QTRY_COMPARE_WITH_TIMEOUT(failure.load(), static_cast<int>(BM_STATUS_UNSUPPORTED), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), BM_SESSION_STOPPED, 2000);
        session.stop();
    }

    void lcdGridAndShadowScaleWithTheDots()
    {
        QCOMPARE(LcdRenderer::gapPixels(2), 0);
        QCOMPARE(LcdRenderer::gapPixels(3), 1);
        QCOMPARE(LcdRenderer::gapPixels(4), 1);
        QCOMPARE(LcdRenderer::gapPixels(5), 1);
        QCOMPARE(LcdRenderer::gapPixels(6), 2);

        // One lit dot at (0,0) of a 2 x 2 frame.
        QImage frame(2, 2, QImage::Format_RGB32);
        frame.fill(LcdRenderer::coreMachOff);
        frame.setPixel(0, 0, LcdRenderer::coreMachOn);
        LcdRenderer lcd;
        QVERIFY(lcd.push(frame));
        const QRgb lit   = LcdRenderer::litColour(LcdSettings::defaultContrast);
        const QRgb unlit = LcdRenderer::unlitColour(LcdSettings::defaultContrast);

        const QImage at4 = lcd.render(4);
        QCOMPARE(at4.size(), QSize(8, 8));
        QCOMPARE(at4.pixel(0, 0) | 0xff000000U, lit);
        QCOMPARE(at4.pixel(3, 0) | 0xff000000U, LcdRenderer::background); // gap
        QCOMPARE(at4.pixel(6, 6) | 0xff000000U, unlit);                   // far unlit dot
        // The shadow falls 1 px down and right: on the gaps beside and below
        // the lit dot, not beyond.
        QVERIFY(qGray(at4.pixel(3, 1)) < qGray(LcdRenderer::background));
        QVERIFY(qGray(at4.pixel(1, 3)) < qGray(LcdRenderer::background));
        QCOMPARE(at4.pixel(4, 1) | 0xff000000U, unlit);

        const QImage at6 = lcd.render(6);
        QCOMPARE(at6.pixel(4, 0) | 0xff000000U, LcdRenderer::background); // 2 px gap
        QVERIFY(qGray(at6.pixel(5, 2)) < qGray(LcdRenderer::background));  // 2 px shadow
        QCOMPARE(at6.pixel(4, 1) | 0xff000000U, LcdRenderer::background); // not 1 px
        QCOMPARE(at6.pixel(6, 1) | 0xff000000U, unlit);

        const QImage at2 = lcd.render(2);
        QCOMPARE(at2.pixel(1, 1) | 0xff000000U, lit); // no gap at 2x
    }

    void lcdEffectOffShowsTheFrameUnchanged()
    {
        QImage frame(3, 2, QImage::Format_RGB32);
        frame.fill(LcdRenderer::coreMachOff);
        frame.setPixel(1, 1, LcdRenderer::coreMachOn);
        LcdRenderer lcd;
        LcdSettings settings;
        settings.enabled = false;
        lcd.setSettings(settings);
        lcd.push(frame);
        QCOMPARE(lcd.render(3), frame.scaled(frame.size() * 3, Qt::IgnoreAspectRatio, Qt::FastTransformation));
    }

    void lcdPersistenceAndContrast()
    {
        QImage off(1, 1, QImage::Format_RGB32);
        off.fill(LcdRenderer::coreMachOff);
        QImage on(1, 1, QImage::Format_RGB32);
        on.fill(LcdRenderer::coreMachOn);
        const QRgb lit   = LcdRenderer::litColour(LcdSettings::defaultContrast);
        const QRgb unlit = LcdRenderer::unlitColour(LcdSettings::defaultContrast);

        LcdRenderer lcd;
        lcd.push(off);
        lcd.push(on); // halfway
        QVERIFY(lcd.settling());
        const QRgb half = lcd.render(1).pixel(0, 0);
        QVERIFY(qGray(half) < qGray(unlit) && qGray(half) > qGray(lit));
        int frames = 0;
        while (lcd.settling() && frames < 20) {
            lcd.settle();
            ++frames;
        }
        QVERIFY(frames > 0 && frames <= 8);
        QCOMPARE(lcd.render(1).pixel(0, 0) | 0xff000000U, lit);

        LcdSettings settings;
        settings.persistence = false;
        lcd.setSettings(settings);
        lcd.push(off);
        QVERIFY(!lcd.settling());
        QCOMPARE(lcd.render(1).pixel(0, 0) | 0xff000000U, unlit);

        // Contrast: darker dots at higher steps; out-of-range steps clamp.
        QVERIFY(qGray(LcdRenderer::litColour(0)) > qGray(LcdRenderer::litColour(4)));
        QVERIFY(qGray(LcdRenderer::unlitColour(0)) >= qGray(LcdRenderer::unlitColour(4)));
        QCOMPARE(LcdRenderer::litColour(-3), LcdRenderer::litColour(0));
        QCOMPARE(LcdRenderer::litColour(99), LcdRenderer::litColour(4));
        // The default dot is 0.30 of the measured background, keeping its hue.
        QCOMPARE(qRed(lit), qRound(qRed(LcdRenderer::background) * 0.30));
        QCOMPARE(qGreen(lit), qRound(qGreen(LcdRenderer::background) * 0.30));
        QVERIFY(qGreen(lit) > qRed(lit) && qGreen(lit) > qBlue(lit));

        QImage strange(1, 1, QImage::Format_RGB32);
        strange.fill(qRgb(10, 10, 200));
        bool recognised = true;
        QCOMPARE(LcdRenderer::mask(strange, &recognised).at(0), 1.0F);
        QVERIFY(!recognised);
    }

    void lcdColoursMatchCoreMachOutput()
    {
        // The mask depends on the two colours CoreMach paints for the M10.
        // This program turns chip 0 on and lights one column of dots, so a
        // change of either colour in CoreMach fails here.
        QByteArray rom(32768, '\0');
        const char program[] = {
            '\x3e', '\x03', '\xd3', '\xb0',            // 81C55: PA, PB outputs
            '\x3e', '\x01', '\xd3', '\xb1',            // PA0: select chip 0
            '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
            '\x3e', '\x39', '\xd3', '\xf0',            // display ON
            '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
            '\x3e', '\xff', '\xd3', '\xf1',            // eight lit dots
            '\xc3', '\x1c', '\x00'                     // loop
        };
        std::memcpy(rom.data(), program, sizeof(program));
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("rtc_time"), 1);
        std::mutex mutex;
        QImage     lastFrame;
        Session    session(profile, rom, {}, [&](const Session::Frame &frame) {
            if (frame.image.isNull())
                return;
            std::lock_guard<std::mutex> lock(mutex);
            lastFrame = frame.image;
        });
        QCOMPARE(session.start(), BM_STATUS_OK);
        QTest::qWait(300);
        QImage frame;
        {
            std::lock_guard<std::mutex> lock(mutex);
            frame = lastFrame;
        }
        session.stop();
        QVERIFY(!frame.isNull());
        QSet<QRgb> colours;
        for (int y = 0; y < frame.height(); ++y)
            for (int x = 0; x < frame.width(); ++x)
                colours.insert(frame.pixel(x, y) | 0xff000000U);
        QCOMPARE(colours, (QSet<QRgb> { LcdRenderer::coreMachOff, LcdRenderer::coreMachOn }));
        bool recognised = false;
        LcdRenderer::mask(frame, &recognised);
        QVERIFY(recognised);
    }

    void buzzerLevelsAndTones()
    {
        constexpr uint64_t ms = 1000000; // CoreMach ticks are nanoseconds here
        constexpr int      perMs = Buzzer::sampleRate / 1000;
        const auto change = [](uint64_t time, int level, uint64_t numerator = 0, uint64_t denominator = 0) {
            bm_audio_change_t result {};
            result.size             = sizeof(result);
            result.time             = time;
            result.level            = level;
            result.tone_numerator   = numerator;
            result.tone_denominator = denominator;
            return result;
        };
        const auto peak = [](const std::vector<int16_t> &samples, size_t from) {
            int result = 0;
            for (size_t index = from; index < samples.size(); ++index)
                result = std::max(result, std::abs(static_cast<int>(samples[index])));
            return result;
        };

        Buzzer               buzzer(UINT64_C(1000000000));
        std::vector<int16_t> samples;
        buzzer.restart(0);
        buzzer.render(10 * ms, samples);
        QCOMPARE(samples.size(), size_t(10 * perMs));
        QCOMPARE(peak(samples, 0), 0);

        // A step clicks and then decays: a level held high is silent.
        buzzer.change(change(10 * ms, 1));
        buzzer.render(510 * ms, samples);
        QCOMPARE(samples.size(), size_t(510 * perMs));
        QVERIFY(samples[10 * perMs] > 15000);
        QVERIFY(peak(samples, 500 * perMs) < 100);

        // 1 kHz from the timer: a square wave of the full swing.
        samples.clear();
        buzzer.change(change(510 * ms, 0, 1000, 1));
        buzzer.render(810 * ms, samples);
        int crossings = 0;
        for (size_t index = 200 * perMs; index < samples.size(); ++index)
            crossings += (samples[index - 1] < 0) != (samples[index] < 0);
        QVERIFY2(crossings >= 198 && crossings <= 202, qPrintable(QString::number(crossings)));
        QVERIFY(peak(samples, 200 * perMs) > 7500 && peak(samples, 200 * perMs) < 9000);

        // The M10 timer at count 16 (153.6 kHz) cannot be heard.
        samples.clear();
        buzzer.change(change(810 * ms, 0, 2457600, 16));
        buzzer.render(1310 * ms, samples);
        QVERIFY(peak(samples, 400 * perMs) < 100);

        // Changes before the samples already rendered apply at the next one.
        samples.clear();
        buzzer.change(change(1000 * ms, 0));
        buzzer.render(1311 * ms, samples);
        QCOMPARE(samples.size(), size_t(perMs));
    }

    void wavTapesRoundTrip()
    {
        Tape tape;
        tape.sampleRate = 44100;
        tape.samples    = { 0, 1000, -1000, 32767, -32768 };
        Tape    read;
        QString reason;
        QVERIFY(readWav(writeWav(tape), &read, &reason));
        QCOMPARE(read.sampleRate, 44100u);
        QCOMPARE(read.samples, tape.samples);

        // 8-bit stereo is mixed to mono 16-bit.
        QByteArray stereo = writeWav(tape).left(44);
        stereo[22]        = 2;                        // channels
        stereo[34]        = 8;                        // bits
        stereo.append(QByteArray("\xff\x00\x80\x80", 4)); // (127 + -128) / 2, then 0
        qToLittleEndian<uint32_t>(4, stereo.data() + 40);
        QVERIFY(readWav(stereo, &read));
        QCOMPARE(read.samples, (std::vector<int16_t> { -128, 0 }));

        // 32-bit float and 24-bit WAVE_FORMAT_EXTENSIBLE.
        QByteArray floating = writeWav(tape).left(44);
        floating[20]        = 3;  // IEEE float
        floating[34]        = 32; // bits
        for (const float value : { 0.5f, -1.0f, 2.0f }) {
            char bytes[4];
            std::memcpy(bytes, &value, 4);
            floating.append(bytes, 4);
        }
        qToLittleEndian<uint32_t>(12, floating.data() + 40);
        QVERIFY(readWav(floating, &read, &reason));
        QCOMPARE(read.samples, (std::vector<int16_t> { 16383, -32767, 32767 }));
        QByteArray extensible = writeWav(tape).left(36);
        qToLittleEndian<uint32_t>(40, extensible.data() + 16); // fmt length
        qToLittleEndian<uint16_t>(0xfffe, extensible.data() + 20);
        extensible[34] = 24;
        extensible.append(QByteArray::fromHex("1600180003000000") + QByteArray::fromHex("0100000000001000800000aa00389b71"));
        extensible.append("data", 4);
        extensible.append(QByteArray::fromHex("06000000" "ff7f00" "000080"));
        QVERIFY(readWav(extensible, &read, &reason));
        QCOMPARE(read.samples, (std::vector<int16_t> { 127, -32768 }));

        QByteArray adpcm = writeWav(tape);
        adpcm[20]        = 2; // MS ADPCM
        QVERIFY(!readWav(adpcm, &read, &reason));
        QVERIFY(!readWav("not a wav", &read));
    }

    void quietTapesAreRaised()
    {
        // A 1 kHz tape at 400 with an offset of 300 and one click.
        Tape tape;
        tape.sampleRate = 48000;
        for (int index = 0; index < 48000; ++index)
            tape.samples.push_back(static_cast<int16_t>(300 + ((index / 24) % 2 ? 400 : -400)));
        tape.samples[100] = 30000;
        QCOMPARE(adjustVolume(tape), 16384.0 / 400);
        QCOMPARE(tape.samples[0], int16_t(-16384));
        QCOMPARE(tape.samples[24], int16_t(16384));
        QCOMPARE(tape.samples[100], int16_t(32767));

        // Loud tapes and silence keep their level; the gain stops at 64.
        Tape loud;
        loud.samples = { 9000, -9000, 9000, -9000 };
        QCOMPARE(adjustVolume(loud), 1.0);
        QCOMPARE(loud.samples, (std::vector<int16_t> { 9000, -9000, 9000, -9000 }));
        Tape silent;
        silent.samples = { 0, 0, 0 };
        QCOMPARE(adjustVolume(silent), 1.0);
        Tape faint;
        faint.samples = { 10, -10, 10, -10 };
        QCOMPARE(adjustVolume(faint), 64.0);
        QCOMPARE(faint.samples[0], int16_t(640));
    }

    void tapeRecorderKeepsOnlyMotorRuns()
    {
        constexpr uint64_t ms = 1000000;
        TapeRecorder       recorder(UINT64_C(1000000000));
        recorder.record(true, 0); // motor off: not on tape
        recorder.motor(true, 10 * ms);
        recorder.record(false, 15 * ms);
        recorder.motor(false, 20 * ms);
        recorder.record(true, 30 * ms);
        recorder.motor(true, 40 * ms);
        recorder.motor(false, 41 * ms);
        const auto &samples = recorder.samples();
        QCOMPARE(samples.size(), size_t(11 * 48));
        QCOMPARE(samples[0], TapeRecorder::amplitude);                  // high from 10 ms
        QCOMPARE(samples[5 * 48], int16_t(-TapeRecorder::amplitude));  // low from 15 ms
        QCOMPARE(samples[10 * 48], TapeRecorder::amplitude);            // the second run
    }

    void sessionRecordsAndPlaysTapes()
    {
        // REMOTE on, then SOD toggles about every 0.37 ms.
        static const uint8_t program[] = {
            0x31, 0xf0, 0xff,       // 0040 LXI SP,FFF0h
            0x3e, 0x08, 0xd3, 0xe0, //      REMOTE on
            0x3e, 0xc0, 0x30,       // 0047 SIM: SOD 1
            0xcd, 0x58, 0x00,       //      CALL 0058h
            0x3e, 0x40, 0x30,       //      SIM: SOD 0
            0xcd, 0x58, 0x00,       //      CALL 0058h
            0xc3, 0x47, 0x00,       //      JMP 0047h
            0x00, 0x00,             //
            0x06, 0x40,             // 0058 MVI B,40h
            0x05,                   // 005A DCR B
            0xc2, 0x5a, 0x00,       //      JNZ 005Ah
            0xc9,                   //      RET
        };
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3';
        rom[1] = '\x40';
        std::memcpy(rom.data() + 0x40, program, sizeof(program));
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        Session session(profile, rom, {}, [](const Session::Frame &) {});
        QCOMPARE(session.start(), BM_STATUS_OK);
        QTRY_VERIFY_WITH_TIMEOUT(session.tapeStatus().motor, 2000);

        session.startRecording();
        QTRY_VERIFY_WITH_TIMEOUT(session.tapeStatus().recorded > 48000 / 5, 3000);
        const Tape recorded = session.stopRecording();
        QCOMPARE(recorded.sampleRate, TapeRecorder::sampleRate);
        QVERIFY(recorded.samples.size() >= 48000 / 5);
        const auto highs = std::count(recorded.samples.begin(), recorded.samples.end(), TapeRecorder::amplitude);
        QVERIFY(highs > 0 && highs < static_cast<qsizetype>(recorded.samples.size()));
        QVERIFY(!session.tapeStatus().recording);

        // The recording plays back while the motor runs.
        session.loadTape(recorded);
        QTRY_VERIFY_WITH_TIMEOUT(session.tapeStatus().loaded, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(session.tapeStatus().position > 4800, 3000);
        QCOMPARE(session.tapeStatus().length, uint64_t(recorded.samples.size()));
        session.rewindTape();
        session.ejectTape();
        QTRY_VERIFY_WITH_TIMEOUT(!session.tapeStatus().loaded, 2000);

        // A recording running when the session stops is kept.
        session.startRecording();
        QTest::qWait(100);
        session.stop();
        QVERIFY(!session.stopRecording().samples.empty());
    }

    void tolerantSessionReportsAndKeepsRunning()
    {
        // IN A8h and OUT A8h (no device there), then DSUB (08h), an extended
        // 8085 instruction, in a loop.
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3';
        rom[1] = '\x40';
        static const uint8_t program[] = { 0xdb, 0xa8, 0xd3, 0xa8, 0x08, 0xc3, 0x40, 0x00 };
        std::memcpy(rom.data() + 0x40, program, sizeof(program));
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        std::mutex                      mutex;
        std::vector<Session::Tolerated> reports;
        std::atomic<int>                leds { 0 };
        Session session(profile, rom, {}, [&](const Session::Frame &frame) {
            std::lock_guard<std::mutex> lock(mutex);
            reports.insert(reports.end(), frame.tolerated.begin(), frame.tolerated.end());
            leds.store(frame.keyboardLeds);
        });
        QCOMPARE(session.start(), BM_STATUS_OK);
        const auto details = [&] {
            std::lock_guard<std::mutex> lock(mutex);
            QStringList list;
            for (const auto &event : reports)
                list << toleratedKind(event) + QLatin1Char(' ') + toleratedDetail(event);
            return list;
        };
        QTRY_COMPARE_WITH_TIMEOUT(details().size(), 3, 2000);
        QVERIFY2(details().contains(QStringLiteral("io_read A8h at PC 0040h (DBh)")), qPrintable(details().join('|')));
        QVERIFY(details().contains(QStringLiteral("io_write A8h at PC 0042h (D3h)")));
        QVERIFY(details().contains(QStringLiteral("undocumented_instruction 08h at PC 0044h (08h)")));
        const uint64_t ticks = session.ticks();
        QTRY_VERIFY_WITH_TIMEOUT(session.ticks() > ticks, 2000);
        QCOMPARE(session.state(), BM_SESSION_RUNNING);
        QCOMPARE(details().size(), 3); // once per case

        // CAPS LOCK latches on the M10 and its indicator reaches the frame.
        QCOMPARE(leds.load() & BM_KEYBOARD_LED_CAPS_LOCK, 0);
        session.key(BM_KEY_CAPS_LOCK, true);
        session.key(BM_KEY_CAPS_LOCK, false);
        QTRY_VERIFY_WITH_TIMEOUT((leds.load() & BM_KEYBOARD_LED_CAPS_LOCK) != 0, 2000);
        session.stop();
    }

    void printoutTextFeedsALineOnCr()
    {
        PrintoutText text;
        QCOMPARE(text.convert("10 A\r20 B\r"), QByteArray("10 A\r\n20 B\r\n"));
        // An LF right after a CR, even in the next call, is the same line feed.
        QCOMPARE(text.convert("\nC\n\x0c"), QByteArray("C\n\x0c"));
        QCOMPARE(printoutFileName(QDateTime(QDate(2026, 10, 27), QTime(15, 30, 5))),
                 QStringLiteral("print-20261027-153005.txt"));
    }

    void sessionPrintsWhatTheMachineStrobes()
    {
        // PA carries each byte and E0h bit 1 strobes it (service manual 6.11).
        static const uint8_t strobe[] = { 0x3e, 0x02, 0xd3, 0xe0, 0x3e, 0x00, 0xd3, 0xe0 };
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3';
        rom[1] = '\x40';
        int at = 0x40;
        const auto put = [&](const QByteArray &bytes) {
            std::memcpy(rom.data() + at, bytes.constData(), bytes.size());
            at += bytes.size();
        };
        put(QByteArray::fromHex("3e03d3b0")); // PA and PB outputs
        for (const char c : QByteArray("Hi\r")) {
            put(QByteArray::fromHex("3e") + c + QByteArray::fromHex("d3b1"));
            put(QByteArray(reinterpret_cast<const char *>(strobe), sizeof(strobe)));
        }
        put(QByteArray::fromHex("c3") + char(at & 0xff) + char(at >> 8)); // JMP to itself
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        std::mutex mutex;
        QByteArray printed;
        Session    session(profile, rom, {}, [&](const Session::Frame &frame) {
            std::lock_guard<std::mutex> lock(mutex);
            printed += frame.printed;
        });
        const auto output = [&] {
            std::lock_guard<std::mutex> lock(mutex);
            return printed;
        };
        QCOMPARE(session.start(), BM_STATUS_OK);
        QTRY_COMPARE_WITH_TIMEOUT(output(), QByteArray("Hi\r"), 2000);
        session.stop();
    }

    void sessionReportsPowerOffAndSwitchesOn()
    {
        // P.C.S. (PB4) switches the machine off once; RAM remembers it, so
        // after the power switch (a reset) it stays on.
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3';
        rom[1] = '\x40';
        const QByteArray program = QByteArray::fromHex("3a00f0" "fe55" "ca5500" "3e55" "3200f0"
                                                       "3e02" "d3b0" "3e10" "d3b2" "c35500");
        std::memcpy(rom.data() + 0x40, program.constData(), program.size());
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        std::atomic<int> off { -1 };
        Session session(profile, rom, {}, [&](const Session::Frame &frame) { off.store(frame.poweredOff ? 1 : 0); });
        QCOMPARE(session.start(), BM_STATUS_OK);
        QTRY_COMPARE_WITH_TIMEOUT(off.load(), 1, 2000);
        session.reset();
        QTRY_COMPARE_WITH_TIMEOUT(off.load(), 0, 2000);
        QTest::qWait(200);
        QCOMPARE(off.load(), 0);
        session.stop();
    }

    void serialTextEndsLikeTheM10()
    {
        QCOMPARE(serialText("10 A\n20 B\r\n30 C\r"), QByteArray("10 A\r\n20 B\r\n30 C\r\n\x1a"));
        QCOMPARE(serialText("x\x1a" "after"), QByteArray("x\x1a"));
        QCOMPARE(serialText(QByteArray()), QByteArray("\x1a"));
    }

    void serialWaitsForXonAndEchoes()
    {
        // 9600 bit/s 8N1; XOFF, half a second, XON, then RST 6.5 echoes
        // every character received.
        static const uint8_t program[] = {
            0x31, 0xf0, 0xff,             // 0040 LXI SP,FFF0h
            0x3e, 0x10, 0xd3, 0xb4,       //      timer count 16
            0x3e, 0x40, 0xd3, 0xb5,       //      continuous square
            0x3e, 0xc2, 0xd3, 0xb0,       //      START, PB output
            0x3e, 0x1c, 0xd3, 0xd0,       //      UART 8N1
            0x3e, 0x13, 0xd3, 0xc0,       //      XOFF
            0x01, 0x00, 0xc8,             //      LXI B,C800h: 51200 x 24 clocks
            0x0b, 0x78, 0xb1,             // 005A DCX B, MOV A,B, ORA C
            0xc2, 0x5a, 0x00,             //      JNZ 005Ah
            0x3e, 0x11, 0xd3, 0xc0,       //      XON
            0x3e, 0x0d, 0x30,             //      SIM: RST 6.5 only
            0xfb,                         //      EI
            0xc3, 0x68, 0x00,             // 0068 JMP 0068h
        };
        static const uint8_t echo[] = { 0xdb, 0xc0, 0xd3, 0xc0, 0xfb, 0xc9 }; // IN C0h, OUT C0h, EI, RET
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3';
        rom[1] = '\x40';
        std::memcpy(rom.data() + 0x34, echo, sizeof(echo));
        std::memcpy(rom.data() + 0x40, program, sizeof(program));
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        std::mutex mutex;
        QByteArray received;
        Session    session(profile, rom, {}, [&](const Session::Frame &frame) {
            std::lock_guard<std::mutex> lock(mutex);
            received += frame.serial;
        });
        const auto output = [&] {
            std::lock_guard<std::mutex> lock(mutex);
            return received;
        };
        QCOMPARE(session.start(), BM_STATUS_OK);
        QTRY_COMPARE_WITH_TIMEOUT(output(), QByteArray("\x13"), 2000);

        // XOFF holds the characters back; XON lets them in and they echo.
        session.sendSerial("ABC");
        QTest::qWait(100);
        QCOMPARE(session.serialPending(), qint64(3));
        QTRY_COMPARE_WITH_TIMEOUT(output(), QByteArray("\x13\x11" "ABC"), 5000);
        QCOMPARE(session.serialPending(), qint64(0));
        session.stop();
    }

    void sessionWithoutSoundDeviceKeepsRunning()
    {
        // Built without OpenAL, the test has no sound device: the session
        // reports it once and runs on.
        QByteArray rom(32768, '\0');
        rom[0] = '\xc3';
        Profile profile;
        profile.system = &m10();
        profile.options.insert(QStringLiteral("ram_kib"), 32);
        profile.options.insert(QStringLiteral("language"), 2);
        profile.options.insert(QStringLiteral("rtc_time"), 1);

        std::atomic<int> unavailable { 0 };
        Session session(profile, rom, {}, [&](const Session::Frame &frame) {
            if (frame.notice.kind == Session::Notice::Kind::SoundUnavailable)
                ++unavailable;
        });
        QVERIFY(!AudioOutput::available());
        QCOMPARE(session.start(), BM_STATUS_OK);
        session.setSound(true);
        QTRY_COMPARE_WITH_TIMEOUT(unavailable.load(), 1, 2000);
        const uint64_t ticks = session.ticks();
        QTRY_VERIFY_WITH_TIMEOUT(session.ticks() > ticks, 2000);
        session.setSound(false);
        QCOMPARE(session.state(), BM_SESSION_RUNNING);
        session.stop();
        QCOMPARE(unavailable.load(), 1);
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

        // ESC in the menu (the M10 has no ESC key) must not stop the machine.
        session.key(BM_KEY_ESCAPE, true);
        session.key(BM_KEY_ESCAPE, false);
        const uint64_t afterEscape = session.ticks();
        QTRY_VERIFY_WITH_TIMEOUT(session.ticks() > afterEscape + UINT64_C(500000000), 5000);
        QCOMPARE(session.state(), BM_SESSION_RUNNING);
        const QString output = qEnvironmentVariable("BLUMACH_TEST_OUTPUT_DIR");
        if (!output.isEmpty())
        {
            QVERIFY(frame.scaled(frame.size() * 3).save(QDir(output).filePath(QStringLiteral("m10-frame.png"))));
            LcdRenderer lcd;
            QVERIFY(lcd.push(frame));
            QVERIFY(lcd.render(4).save(QDir(output).filePath(QStringLiteral("m10-lcd-4x.png"))));
        }

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

QTEST_MAIN(CoreMachBridgeTest)
#include "coremach_bridge_test.moc"
