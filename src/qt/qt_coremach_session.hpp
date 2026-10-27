/*
 * BluMach bridge to the CoreMach engine: a running machine session.
 *
 * The session runs on its own thread, paced to wall-clock time, and takes
 * commands from the interface thread. The pacing and the timing of key
 * transitions follow CoreMach's portable launcher (apps/qt-portable,
 * session_worker.cpp).
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_SESSION_HPP
#define QT_COREMACH_SESSION_HPP

#include "qt_coremach_audio.hpp"
#include "qt_coremach_profile.hpp"
#include "qt_coremach_tape.hpp"

#include <QByteArray>
#include <QHash>
#include <QImage>

#include <blumach/engine/input.h>
#include <blumach/runtime/runtime.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace CoreMachBridge {

class Session final {
public:
    // Something the machine rejected or failed to do without stopping.
    struct Notice {
        enum class Kind {
            None,
            KeyWithoutPosition, // the machine has no such key
            InputRejected,
            VideoFailed,
            CommandFailed, // pause, resume or reset
            SoundUnavailable, // no sound device could be opened
        };
        Kind          kind   = Kind::None;
        bm_status_t   status = BM_STATUS_OK;
        bm_key_code_t key    = static_cast<bm_key_code_t>(0);
    };
    // A fallback the machine applied instead of failing (tolerant host).
    struct Tolerated {
        bm_tolerance_event_kind_t kind    = BM_TOLERANCE_UNMODELLED_IO_READ;
        uint32_t                  flags   = 0;
        QString                   device;
        QString                   note;
        uint64_t                  pc      = 0;
        uint32_t                  opcode  = 0;
        uint64_t                  address = 0;
        uint64_t                  value   = 0;
    };
    struct Frame {
        QImage             image;
        bm_session_state_t state = BM_SESSION_NEW;
        // Not OK only when emulation itself failed and the session stopped.
        bm_status_t status = BM_STATUS_OK;
        Notice      notice;
        // Characters the machine sent on its serial port since the last frame.
        QByteArray serial;
        // Fallbacks since the last frame.
        std::vector<Tolerated> tolerated;
        // bm_keyboard_led_t bits; 0 when the machine reports none.
        uint8_t keyboardLeds = 0;
    };
    using FrameHandler = std::function<void(const Frame &)>;

    // Called on the session thread; the handler must not block.
    // A tolerant session reports fallbacks (Frame::tolerated) where a strict
    // one stops with the machine's failure.
    Session(const Profile &profile, const QByteArray &firmware, const States &states,
            FrameHandler handler, bool tolerant = true);
    ~Session();
    Session(const Session &)            = delete;
    Session &operator=(const Session &) = delete;

    // Configures and starts the machine; returns once it runs or failed.
    bm_status_t start();
    void        pause();
    void        resume();
    void        reset();
    void        key(bm_key_code_t key, bool pressed);
    // Plays the machine's sound or keeps it silent (off until called).
    void setSound(bool on);
    // Queues characters for the machine's serial port. They go in at its
    // line rate and wait while the machine has sent XOFF, until XON; a
    // reset drops what has not gone in yet.
    void sendSerial(const QByteArray &data);
    void cancelSerial();
    // Characters queued and not yet taken by the machine.
    qint64 serialPending() const { return m_serialPending.load(); }

    // Cassette: a tape plays while the machine's motor runs; recording
    // keeps what the machine sends while the motor runs.
    void loadTape(const Tape &tape);
    void rewindTape();
    void ejectTape();
    void startRecording();
    // Stops recording and returns the recording.
    Tape stopRecording();
    struct TapeStatus {
        bool     loaded    = false;
        bool     motor     = false;
        bool     recording = false;
        uint64_t position  = 0; // samples
        uint64_t length    = 0;
        uint32_t rate      = 0;
        uint64_t recorded  = 0; // samples at TapeRecorder::sampleRate
    };
    TapeStatus tapeStatus() const;
    // Current persistent states, saved from the running machine.
    States capture();
    // Stops the machine and returns its final persistent states.
    States stop();

    bm_session_state_t state() const { return m_state.load(); }
    uint64_t           ticks() const { return m_ticks.load(); }
    // Emulated time as a percentage of wall-clock time; negative when idle.
    double realTimePercent() const { return m_realTime.load(); }

private:
    enum class CommandKind {
        Pause,
        Resume,
        Reset,
        Key,
        Sound,
        SerialSend,
        SerialCancel,
        TapeLoad,
        TapeRewind,
        TapeEject,
        RecordStart,
        RecordStop,
        Capture,
        Stop
    };
    struct Command {
        CommandKind      kind;
        bm_input_event_t input {};
        bool             on = false; // Sound
        QByteArray       data;       // SerialSend
        Tape             tape;       // TapeLoad
    };
    struct Machine;
    static void serialTransmitted(void *context, uint32_t port, uint8_t data, bm_tick_t time);
    static void tolerated(void *context, const bm_tolerance_event_t *event);
    static void tapeMotor(void *context, uint32_t port, int on, bm_tick_t time);
    static void tapeRecord(void *context, uint32_t port, int level, bm_tick_t time);
    // Lends the tape to the machine from position, or ejects it.
    void        insertTape(bm_session_t *session, size_t position);
    // Publishes the tape position and recording length for tapeStatus().
    void        updateTapeStatus(bm_session_t *session);

    void   enqueue(const Command &command);
    void   run();
    bool   process(bm_session_t *session);
    bool   stopAfterFailure(bm_session_t *session, bm_status_t status);
    // Renders the sound up to the session's time and plays it when on.
    void   playSound(bm_session_t *session);
    // Offers the queued serial characters the machine has room for.
    void   feedSerial(bm_session_t *session);
    States captureFrom(bm_session_t *session) const;
    void   publish(bm_session_t *session, bm_status_t status, bool render);
    void   publish(bm_session_t *session, bm_status_t status, bool render, const Notice &notice);

    std::unique_ptr<Machine> m_machine;
    FrameHandler             m_handler;
    std::thread              m_thread;

    std::mutex              m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_done;
    std::deque<Command>     m_commands;
    bool                    m_started     = false;
    bm_status_t             m_startStatus = BM_STATUS_INVALID_STATE;
    uint64_t                m_captureRequests = 0;
    uint64_t                m_captureReplies  = 0;
    States                  m_captured;
    uint64_t                m_recordRequests = 0;
    uint64_t                m_recordReplies  = 0;
    Tape                    m_recorded;
    bool                    m_finished = false;

    std::atomic<bm_session_state_t> m_state { BM_SESSION_NEW };
    std::atomic<uint64_t>           m_ticks { 0 };
    std::atomic<double>             m_realTime { -1.0 };

    // Pacing, owned by the session thread.
    uint64_t m_paceOrigin  = 0; // steady clock, ns
    uint64_t m_paceTicks   = 0; // ticks run since m_paceOrigin
    uint64_t m_lastFrame   = 0;
    uint64_t m_speedOrigin = 0;
    uint64_t m_speedTicks  = 0;

    // Sound, owned by the session thread.
    AudioOutput          m_audio;
    std::vector<int16_t> m_samples;

    // Serial port, owned by the session thread.
    QByteArray          m_serialOut;  // queued for the machine
    QByteArray          m_serialIn;   // sent by the machine, not yet published
    std::vector<Tolerated> m_tolerated; // reported, not yet published
    bool                m_xoff = false;
    std::atomic<qint64> m_serialPending { 0 };

    // Cassette, owned by the session thread.
    Tape                          m_tape; // lent to the machine while loaded
    bool                          m_tapeLoaded = false;
    std::unique_ptr<TapeRecorder> m_recorder;
    std::atomic<bool>             m_tapeStatusLoaded { false };
    std::atomic<bool>             m_motor { false };
    std::atomic<uint64_t>         m_tapePosition { 0 };
    std::atomic<uint64_t>         m_tapeLength { 0 };
    std::atomic<uint32_t>         m_tapeRate { 0 };
    std::atomic<uint64_t>         m_recordedSamples { 0 };
    std::atomic<bool>             m_recording { false };
};

// The event-log kind and detail of a fallback, such as "io_read" and
// "A8h at PC 0000h (DBh)".
QString toleratedKind(const Session::Tolerated &event);
QString toleratedDetail(const Session::Tolerated &event);

// A text file as the M10's own SAVE "COM:" sends it: lines end in CR LF and
// the text ends with Ctrl+Z, which LOAD "COM:" waits for.
QByteArray serialText(const QByteArray &file);

}

#endif
