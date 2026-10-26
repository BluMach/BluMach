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
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
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
    m_session = std::make_unique<Session>(profile, firmware, states, [this](const Session::Frame &frame) {
        QMetaObject::invokeMethod(this, [this, frame] { present(frame); }, Qt::QueuedConnection);
    });

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
    updateStatus();
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
    m_status->setText(text);
}

}
