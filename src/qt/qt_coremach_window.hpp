/*
 * BluMach bridge to the CoreMach engine: the machine window.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_WINDOW_HPP
#define QT_COREMACH_WINDOW_HPP

#include "qt_coremach_display.hpp"
#include "qt_coremach_session.hpp"

#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QSet>
#include <QTimer>

#include <memory>

class BluMachCatalog;
class QActionGroup;
class QLabel;

namespace CoreMachBridge {

// The window of a running CoreMach machine. It owns the session and saves
// the persistent states in the machine folder while it runs and on exit.
class Window final : public QMainWindow {
    Q_OBJECT
public:
    Window(const BluMachCatalog &catalog, const QString &machineName, const QString &machineFolder,
           const QString &configPath, const Profile &profile, const QByteArray &firmware,
           const States &states);
    ~Window() override;

    bm_status_t start();
    bool        paused() const;
    // Exit status of the process once the window has closed.
    int exitCode() const { return m_exitCode; }

signals:
    void runningStateChanged(bool paused);

public slots:
    void togglePause();
    void hardReset();
    // Stops the machine without asking, saving its states.
    void forceShutdown();
    void showSettingsNotice();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void present(const Session::Frame &frame);
    void handleNotice(const Session::Notice &notice);
    void sendKey(bm_key_code_t key, bool pressed);
    void releaseKeys();
    void setPausedState(bool paused);
    void saveStates(const States &states);
    void shutdown();
    void updateStatus();
    // Applies and saves a display preference ([CoreMach] lcd_* key).
    void setLcd(const LcdSettings &settings, const char *key, const QString &value);
    // Reports a preference that could not be written to the configuration.
    void showPreferenceSaveFailure(const char *textKey);
    // Serial port: sends a text file, or saves what the machine sends.
    void sendSerialFile();
    void setSerialCapture(bool on);
    // Cassette: plays a WAV tape, or records what the machine saves.
    void insertTape();
    void setTapeRecording(bool on);
    void saveRecording();

    const BluMachCatalog    &m_catalog;
    QString                  m_name;
    QString                  m_folder;
    QString                  m_configPath;
    std::unique_ptr<Session> m_session;
    Display                 *m_display  = nullptr;
    QLabel                  *m_status   = nullptr;
    QLabel                  *m_capsLock = nullptr;
    QSet<QString>            m_toleratedShown; // this session
    QAction                 *m_pause    = nullptr;
    QAction                 *m_serialCapture = nullptr;
    QAction                 *m_serialCancel  = nullptr;
    QFile                    m_capture; // open while saving received data
    qint64                   m_capturedBytes = 0;
    QAction                 *m_tapeRecord = nullptr;
    QString                  m_recordPath; // while recording
    QTimer                   m_saveTimer;
    QTimer                   m_statusTimer;
    QSet<int>                m_pressed;
    QSet<int>                m_missingKeysShown; // this session
    EventLog                 m_events;
    QString                  m_systemName;
    bool                     m_paused   = false;
    bool                     m_stopped  = false;
    bool                     m_failed   = false;
    bool                     m_force    = false;
    int                      m_exitCode = 0;
};

}

#endif
