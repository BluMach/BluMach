/*
 * BluMach bridge to the CoreMach engine: the machine window.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_window.hpp"
#include "qt_blumach_catalog.hpp"
#include "qt_coremach_keys.hpp"

#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStatusBar>

#include <cstdio>

extern "C" {
#include <86box/86box.h>
#include <86box/plat.h>
}

namespace CoreMachBridge {
namespace {
    constexpr int savePeriodMs   = 60 * 1000;
    constexpr int statusPeriodMs = 500;

    bool unattended()
    {
        return qEnvironmentVariable("BLUMACH_UNATTENDED") == QStringLiteral("1");
    }
}

Window::Window(const BluMachCatalog &catalog, const QString &machineName, const QString &machineFolder,
               const QString &configPath, const Profile &profile, const QByteArray &firmware,
               const States &states)
    : m_catalog(catalog)
    , m_name(machineName)
    , m_folder(machineFolder)
    , m_configPath(configPath)
    , m_events(machineFolder)
    , m_systemName(profile.system ? QString::fromUtf8(profile.system->name) : QString())
{
    // Tolerant unless the machine's [CoreMach] section says tolerant = 0.
    const bool tolerant = !profile.system || readDisplayPreferences(configPath, *profile.system).tolerant;
    m_session = std::make_unique<Session>(
        profile, firmware, states,
        [this](const Session::Frame &frame) {
            QMetaObject::invokeMethod(this, [this, frame] { present(frame); }, Qt::QueuedConnection);
        },
        tolerant);

    setWindowTitle(QStringLiteral("%1 — BluMach").arg(m_name));
    m_display = new Display(this);
    setCentralWidget(m_display);
    connect(m_display, &Display::key, this, &Window::sendKey);
    connect(m_display, &Display::focusLost, this, &Window::releaseKeys);

    auto *action = menuBar()->addMenu(m_catalog.text(QStringLiteral("coremach.menu.action")));
    m_pause      = action->addAction(m_catalog.text(QStringLiteral("coremach.menu.pause")));
    m_pause->setCheckable(true);
    connect(m_pause, &QAction::triggered, this, &Window::togglePause);
    connect(action->addAction(m_catalog.text(QStringLiteral("coremach.menu.reset"))), &QAction::triggered,
            this, &Window::hardReset);
    action->addSeparator();
    // Sound, saved per machine; builds without OpenAL have none.
    auto *sound = action->addAction(m_catalog.text(QStringLiteral("coremach.menu.sound")));
    sound->setCheckable(true);
    sound->setEnabled(AudioOutput::available());
    const bool soundOn = AudioOutput::available()
        && (!profile.system || readDisplayPreferences(m_configPath, *profile.system).sound);
    sound->setChecked(soundOn);
    m_session->setSound(soundOn);
    connect(sound, &QAction::toggled, this, [this](bool on) {
        m_session->setSound(on);
        if (!writeCoreMachValue(m_configPath, QStringLiteral("sound"), on ? QStringLiteral("1") : QStringLiteral("0")))
            showPreferenceSaveFailure("coremach.sound_save_failed");
    });
    // Serial port: text files in, received characters out to a file.
    if (profile.system && profile.system->serialPort) {
        auto *serial = action->addMenu(m_catalog.text(QStringLiteral("coremach.menu.serial")));
        connect(serial->addAction(m_catalog.text(QStringLiteral("coremach.menu.serial_send"))), &QAction::triggered,
                this, &Window::sendSerialFile);
        m_serialCapture = serial->addAction(m_catalog.text(QStringLiteral("coremach.menu.serial_capture")));
        m_serialCapture->setCheckable(true);
        connect(m_serialCapture, &QAction::toggled, this, &Window::setSerialCapture);
        m_serialCancel = serial->addAction(m_catalog.text(QStringLiteral("coremach.menu.serial_cancel")));
        m_serialCancel->setEnabled(false);
        connect(m_serialCancel, &QAction::triggered, this, [this] {
            m_session->cancelSerial();
            m_serialCancel->setEnabled(false);
        });
    }
    // Cassette: WAV tapes in and out.
    if (profile.system && profile.system->cassette) {
        auto *cassette = action->addMenu(m_catalog.text(QStringLiteral("coremach.menu.cassette")));
        connect(cassette->addAction(m_catalog.text(QStringLiteral("coremach.menu.cassette_insert"))),
                &QAction::triggered, this, &Window::insertTape);
        connect(cassette->addAction(m_catalog.text(QStringLiteral("coremach.menu.cassette_rewind"))),
                &QAction::triggered, this, [this] { m_session->rewindTape(); });
        connect(cassette->addAction(m_catalog.text(QStringLiteral("coremach.menu.cassette_eject"))),
                &QAction::triggered, this, [this] { m_session->ejectTape(); });
        cassette->addSeparator();
        m_tapeRecord = cassette->addAction(m_catalog.text(QStringLiteral("coremach.menu.cassette_record")));
        m_tapeRecord->setCheckable(true);
        connect(m_tapeRecord, &QAction::toggled, this, &Window::setTapeRecording);
    }
    action->addSeparator();
    connect(action->addAction(m_catalog.text(QStringLiteral("coremach.menu.exit"))), &QAction::triggered,
            this, &QWidget::close);

    auto *view   = menuBar()->addMenu(m_catalog.text(QStringLiteral("coremach.menu.view")));
    auto *scales = new QActionGroup(this);
    for (int scale = Display::minimumScale; scale <= Display::maximumScale; ++scale) {
        auto *item = view->addAction(m_catalog.text(QStringLiteral("coremach.menu.scale")).arg(scale));
        item->setCheckable(true);
        item->setChecked(scale == m_display->scale());
        scales->addAction(item);
        connect(item, &QAction::triggered, this, [this, scale] {
            m_display->setScale(scale);
            if (!isMaximized() && !isFullScreen())
                adjustSize();
        });
    }

    // LCD look, saved per machine. Contrast only where the machine has the
    // knob (the M10: right side panel, operations guide p. 1-10).
    LcdSettings lcd;
    if (profile.system) {
        const auto preferences = readDisplayPreferences(m_configPath, *profile.system);
        lcd.enabled            = preferences.lcdEffect;
        lcd.persistence        = preferences.lcdPersistence;
        lcd.contrast           = preferences.lcdContrast;
    }
    m_display->setLcdSettings(lcd);
    view->addSeparator();
    auto *effect = view->addAction(m_catalog.text(QStringLiteral("coremach.menu.lcd_effect")));
    effect->setCheckable(true);
    effect->setChecked(lcd.enabled);
    auto *persistence = view->addAction(m_catalog.text(QStringLiteral("coremach.menu.lcd_persistence")));
    persistence->setCheckable(true);
    persistence->setChecked(lcd.persistence);
    persistence->setEnabled(lcd.enabled);
    QMenu *contrast = nullptr;
    if (profile.system && profile.system->contrastControl) {
        contrast       = view->addMenu(m_catalog.text(QStringLiteral("coremach.menu.contrast")));
        auto *contrasts = new QActionGroup(this);
        for (int step = 0; step < LcdSettings::contrastSteps; ++step) {
            auto *item = contrast->addAction(m_catalog.text(QStringLiteral("coremach.menu.contrast_step"))
                                                 .arg(step + 1)
                                                 .arg(LcdSettings::contrastSteps));
            item->setCheckable(true);
            item->setChecked(step == lcd.contrast);
            contrasts->addAction(item);
            connect(item, &QAction::triggered, this, [this, step] {
                LcdSettings settings = m_display->lcdSettings();
                settings.contrast    = step;
                setLcd(settings, "lcd_contrast", QString::number(step));
            });
        }
        contrast->setEnabled(lcd.enabled);
    }
    connect(effect, &QAction::toggled, this, [this, persistence, contrast](bool on) {
        LcdSettings settings = m_display->lcdSettings();
        settings.enabled     = on;
        persistence->setEnabled(on);
        if (contrast)
            contrast->setEnabled(on);
        setLcd(settings, "lcd_effect", on ? QStringLiteral("1") : QStringLiteral("0"));
    });
    connect(persistence, &QAction::toggled, this, [this](bool on) {
        LcdSettings settings = m_display->lcdSettings();
        settings.persistence = on;
        setLcd(settings, "lcd_persistence", on ? QStringLiteral("1") : QStringLiteral("0"));
    });

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);
    m_capsLock = new QLabel(m_catalog.text(QStringLiteral("coremach.status.caps_lock")), this);
    m_capsLock->setVisible(false);
    statusBar()->addPermanentWidget(m_capsLock);
    statusBar()->setSizeGripEnabled(false);

    m_saveTimer.setInterval(savePeriodMs);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        if (!m_stopped)
            saveStates(m_session->capture());
    });
    m_statusTimer.setInterval(statusPeriodMs);
    connect(&m_statusTimer, &QTimer::timeout, this, &Window::updateStatus);
}

Window::~Window()
{
    shutdown();
}

bm_status_t
Window::start()
{
    const auto status = m_session->start();
    if (status == BM_STATUS_OK) {
        setPausedState(false);
        m_saveTimer.start();
        m_statusTimer.start();
        updateStatus();
        m_display->setFocus();
    } else
        m_stopped = true;
    return status;
}

bool
Window::paused() const
{
    return m_paused;
}

void
Window::setPausedState(bool paused)
{
    m_paused = paused;
    dopause  = paused ? 1 : 0; // read by the manager connection
    m_pause->setChecked(paused);
    emit runningStateChanged(paused);
    updateStatus();
}

void
Window::togglePause()
{
    if (m_stopped)
        return;
    releaseKeys();
    if (m_paused)
        m_session->resume();
    else {
        m_session->pause();
        saveStates(m_session->capture());
    }
    setPausedState(!m_paused);
}

void
Window::hardReset()
{
    if (m_stopped)
        return;
    releaseKeys();
    m_session->reset();
}

void
Window::forceShutdown()
{
    m_force = true;
    close();
}

void
Window::showSettingsNotice()
{
    const QString text = m_catalog.text(QStringLiteral("coremach.settings_unavailable")).arg(m_name);
    std::fprintf(stderr, "BluMach: CoreMach settings_unavailable: %s\n", text.toUtf8().constData());
    if (!unattended())
        QMessageBox::information(this, m_name, text);
}

void
Window::closeEvent(QCloseEvent *event)
{
    if (!m_stopped && !m_force && confirm_exit && confirm_exit_cmdl && !unattended()) {
        const auto answer = QMessageBox::question(this, m_name,
                                                  m_catalog.text(QStringLiteral("coremach.confirm_exit")).arg(m_name));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    shutdown();
    event->accept();
}

void
Window::present(const Session::Frame &frame)
{
    if (m_stopped)
        return;
    if (!frame.image.isNull())
        m_display->setFrame(frame.image);
    m_capsLock->setVisible((frame.keyboardLeds & BM_KEYBOARD_LED_CAPS_LOCK) != 0);
    for (const auto &event : frame.tolerated) {
        // Logged once per machine folder; shown once per session.
        const QString kind   = toleratedKind(event);
        const QString detail = toleratedDetail(event);
        QString       context = event.device + QStringLiteral(": ") + event.note;
        if (event.flags & BM_TOLERANCE_FLAG_ASSUMED_BEHAVIOUR)
            context += QStringLiteral(" [assumed behaviour]");
        if (m_events.record(kind, detail, context))
            std::fprintf(stderr, "BluMach: CoreMach tolerated %s %s\n", kind.toUtf8().constData(),
                         detail.toUtf8().constData());
        if (!m_toleratedShown.contains(kind)) {
            m_toleratedShown.insert(kind);
            statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.tolerated"))
                                         .arg(m_systemName, kind + QLatin1Char(' ') + detail,
                                              QDir::toNativeSeparators(m_events.path())),
                                     10000);
        }
    }
    if (!frame.serial.isEmpty() && m_capture.isOpen()) {
        if (m_capture.write(frame.serial) == frame.serial.size() && m_capture.flush())
            m_capturedBytes += frame.serial.size();
        else {
            const QString text = m_catalog.text(QStringLiteral("coremach.serial_write_failed"))
                                     .arg(m_name, QDir::toNativeSeparators(m_capture.fileName()));
            std::fprintf(stderr, "BluMach: CoreMach serial_write_failed: %s\n", text.toUtf8().constData());
            m_capture.close();
            if (m_serialCapture) {
                const QSignalBlocker blocker(m_serialCapture);
                m_serialCapture->setChecked(false);
            }
            statusBar()->showMessage(text, 10000);
        }
    }
    if (frame.notice.kind != Session::Notice::Kind::None)
        handleNotice(frame.notice);
    // Only a failed emulation run reaches here as a status.
    if (frame.status != BM_STATUS_OK && !m_failed) {
        m_failed = true;
        shutdown();
        const QString text = m_catalog.text(QStringLiteral("coremach.session_failed"))
                                 .arg(m_name, QString::number(static_cast<int>(frame.status)));
        std::fprintf(stderr, "BluMach: CoreMach session_failed: %s\n", text.toUtf8().constData());
        if (!unattended())
            QMessageBox::warning(this, m_name, text);
        close();
    }
}

void
Window::handleNotice(const Session::Notice &notice)
{
    using Kind = Session::Notice::Kind;
    const QString status = QStringLiteral("status %1").arg(static_cast<int>(notice.status));
    switch (notice.kind) {
        case Kind::None:
            return;
        case Kind::KeyWithoutPosition: {
            const QString name = keyName(notice.key);
            m_events.record(QStringLiteral("key_without_position"), name, m_systemName);
            if (m_missingKeysShown.contains(static_cast<int>(notice.key)))
                return;
            m_missingKeysShown.insert(static_cast<int>(notice.key));
            statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.key_missing")).arg(name, m_systemName), 5000);
            return;
        }
        case Kind::InputRejected:
            if (m_events.record(QStringLiteral("input_rejected"), keyName(notice.key), status))
                std::fprintf(stderr, "BluMach: CoreMach input rejected (%s)\n", status.toUtf8().constData());
            return;
        case Kind::VideoFailed:
            if (m_events.record(QStringLiteral("video_failed"), status))
                std::fprintf(stderr, "BluMach: CoreMach frame not drawn (%s)\n", status.toUtf8().constData());
            return;
        case Kind::CommandFailed:
            if (m_events.record(QStringLiteral("command_failed"), status))
                std::fprintf(stderr, "BluMach: CoreMach command failed (%s)\n", status.toUtf8().constData());
            return;
        case Kind::SoundUnavailable:
            if (m_events.record(QStringLiteral("sound_unavailable"), QStringLiteral("OpenAL")))
                std::fprintf(stderr, "BluMach: CoreMach sound device unavailable\n");
            statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.sound_unavailable")), 5000);
            return;
    }
}

void
Window::sendKey(bm_key_code_t key, bool pressed)
{
    if (m_stopped || m_paused)
        return;
    const int code = static_cast<int>(key);
    if (pressed == m_pressed.contains(code))
        return;
    if (pressed)
        m_pressed.insert(code);
    else
        m_pressed.remove(code);
    m_session->key(key, pressed);
}

void
Window::releaseKeys()
{
    if (!m_stopped)
        for (const int code : std::as_const(m_pressed))
            m_session->key(static_cast<bm_key_code_t>(code), false);
    m_pressed.clear();
}

void
Window::saveStates(const States &states)
{
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        const QString path = stateFile(m_folder, it.key().toLatin1().constData());
        if (CoreMachBridge::saveState(path, it.value()))
            continue;
        const QString text = m_catalog.text(QStringLiteral("coremach.save_failed"))
                                 .arg(m_name, QDir::toNativeSeparators(QFileInfo(path).absolutePath()));
        std::fprintf(stderr, "BluMach: CoreMach save_failed: %s\n", text.toUtf8().constData());
        statusBar()->showMessage(text, 10000);
    }
}

void
Window::shutdown()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_saveTimer.stop();
    m_statusTimer.stop();
    m_pressed.clear();
    saveStates(m_session->stop());
    if (m_capture.isOpen())
        m_capture.close();
    saveRecording(); // a recording still running goes to its file
    updateStatus();
}

namespace {
    QString duration(uint64_t samples, uint32_t rate)
    {
        const uint64_t seconds = rate ? samples / rate : 0;
        return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
}

void
Window::insertTape()
{
    if (m_stopped)
        return;
    const QString title = m_catalog.text(QStringLiteral("coremach.menu.cassette_insert")).remove(QLatin1Char('&'));
    const QString path  = QFileDialog::getOpenFileName(this, title, m_folder,
                                                       m_catalog.text(QStringLiteral("coremach.cassette_filter")));
    if (path.isEmpty())
        return;
    QFile   file(path);
    Tape    tape;
    QString reason;
    if (!file.open(QIODevice::ReadOnly) || !readWav(file.readAll(), &tape, &reason)) {
        std::fprintf(stderr, "BluMach: CoreMach tape not loaded: %s\n", reason.toUtf8().constData());
        statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.cassette_unreadable"))
                                     .arg(QDir::toNativeSeparators(path)),
                                 10000);
        return;
    }
    m_session->loadTape(tape);
    updateStatus();
}

void
Window::setTapeRecording(bool on)
{
    if (!on) {
        saveRecording();
        return;
    }
    const QString title = m_catalog.text(QStringLiteral("coremach.menu.cassette_record")).remove(QLatin1Char('&'));
    const QString path  = QFileDialog::getSaveFileName(this, title, QDir(m_folder).filePath(QStringLiteral("tape.wav")),
                                                       m_catalog.text(QStringLiteral("coremach.cassette_filter")));
    if (path.isEmpty() || m_stopped) {
        const QSignalBlocker blocker(m_tapeRecord);
        m_tapeRecord->setChecked(false);
        return;
    }
    m_recordPath = path;
    m_session->startRecording();
    updateStatus();
}

void
Window::saveRecording()
{
    if (m_recordPath.isEmpty())
        return;
    const QString path = m_recordPath;
    m_recordPath.clear();
    if (m_tapeRecord) {
        const QSignalBlocker blocker(m_tapeRecord);
        m_tapeRecord->setChecked(false);
    }
    const Tape tape = m_session->stopRecording();
    QSaveFile  file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(writeWav(tape)) >= 0 && file.commit()) {
        statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.cassette_saved"))
                                     .arg(duration(tape.samples.size(), tape.sampleRate),
                                          QDir::toNativeSeparators(path)),
                                 10000);
        return;
    }
    const QString text = m_catalog.text(QStringLiteral("coremach.cassette_save_failed"))
                             .arg(m_name, QDir::toNativeSeparators(path));
    std::fprintf(stderr, "BluMach: CoreMach cassette_save_failed: %s\n", text.toUtf8().constData());
    statusBar()->showMessage(text, 10000);
}

void
Window::sendSerialFile()
{
    if (m_stopped)
        return;
    const QString title = m_catalog.text(QStringLiteral("coremach.menu.serial_send")).remove(QLatin1Char('&'));
    const QString path  = QFileDialog::getOpenFileName(this, title, m_folder,
                                                       m_catalog.text(QStringLiteral("coremach.serial_text_filter")));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.serial_open_failed"))
                                     .arg(QDir::toNativeSeparators(path)),
                                 10000);
        return;
    }
    m_session->sendSerial(serialText(file.readAll()));
    updateStatus();
}

void
Window::setSerialCapture(bool on)
{
    if (!on) {
        if (!m_capture.isOpen())
            return;
        m_capture.close();
        statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.serial_capture_saved"))
                                     .arg(QString::number(m_capturedBytes),
                                          QDir::toNativeSeparators(m_capture.fileName())),
                                 10000);
        return;
    }
    const QString title = m_catalog.text(QStringLiteral("coremach.menu.serial_capture")).remove(QLatin1Char('&'));
    const QString path  = QFileDialog::getSaveFileName(this, title, QDir(m_folder).filePath(QStringLiteral("serial.txt")),
                                                       m_catalog.text(QStringLiteral("coremach.serial_text_filter")));
    m_capture.setFileName(path);
    if (path.isEmpty() || !m_capture.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (!path.isEmpty())
            statusBar()->showMessage(m_catalog.text(QStringLiteral("coremach.serial_open_failed"))
                                         .arg(QDir::toNativeSeparators(path)),
                                     10000);
        const QSignalBlocker blocker(m_serialCapture);
        m_serialCapture->setChecked(false);
        return;
    }
    m_capturedBytes = 0;
}

void
Window::setLcd(const LcdSettings &settings, const char *key, const QString &value)
{
    m_display->setLcdSettings(settings);
    if (!writeCoreMachValue(m_configPath, QString::fromLatin1(key), value))
        showPreferenceSaveFailure("coremach.display_save_failed");
}

void
Window::showPreferenceSaveFailure(const char *textKey)
{
    const QString key  = QString::fromLatin1(textKey);
    const QString text = m_catalog.text(key).arg(m_name, QDir::toNativeSeparators(m_configPath));
    std::fprintf(stderr, "BluMach: CoreMach %s: %s\n", key.mid(key.indexOf('.') + 1).toUtf8().constData(),
                 text.toUtf8().constData());
    statusBar()->showMessage(text, 10000);
}

void
Window::updateStatus()
{
    if (!m_status)
        return;
    QString text;
    if (m_stopped)
        text = m_catalog.text(QStringLiteral("coremach.status.stopped"));
    else if (m_paused)
        text = m_catalog.text(QStringLiteral("coremach.status.paused"));
    else {
        text = m_catalog.text(QStringLiteral("coremach.status.running"));
        const double speed = m_session->realTimePercent();
        if (speed >= 0.0)
            text += QStringLiteral(" · ")
                + m_catalog.text(QStringLiteral("coremach.status.speed")).arg(QString::number(speed, 'f', 0));
    }
    const qint64 pending = m_stopped ? 0 : m_session->serialPending();
    if (pending > 0)
        text += QStringLiteral(" · ")
            + m_catalog.text(QStringLiteral("coremach.status.serial_sending")).arg(QString::number(pending));
    if (m_serialCancel)
        m_serialCancel->setEnabled(pending > 0);
    if (!m_stopped) {
        const auto tape = m_session->tapeStatus();
        if (tape.loaded)
            text += QStringLiteral(" · ")
                + m_catalog.text(QStringLiteral("coremach.status.tape"))
                      .arg(duration(tape.position, tape.rate), duration(tape.length, tape.rate));
        if (tape.motor)
            text += QStringLiteral(" · ") + m_catalog.text(QStringLiteral("coremach.status.tape_motor"));
        if (tape.recording)
            text += QStringLiteral(" · ")
                + m_catalog.text(QStringLiteral("coremach.status.tape_recording"))
                      .arg(duration(tape.recorded, TapeRecorder::sampleRate));
    }
    m_status->setText(text);
}

}
