/*
 * BluMach bridge to the CoreMach engine: the machine window.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_window.hpp"
#include "qt_blumach_catalog.hpp"

#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QLabel>
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
               const Profile &profile, const QByteArray &firmware, const States &states)
    : m_catalog(catalog)
    , m_name(machineName)
    , m_folder(machineFolder)
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
