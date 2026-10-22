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

#include "qt_coremach_profile.hpp"

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
    struct Frame {
        QImage             image;
        bm_session_state_t state = BM_SESSION_NEW;
        bm_status_t        status = BM_STATUS_OK;
    };
    using FrameHandler = std::function<void(const Frame &)>;

    // Called on the session thread; the handler must not block.
    Session(const Profile &profile, const QByteArray &firmware, const States &states,
            FrameHandler handler);
    ~Session();
    Session(const Session &)            = delete;
    Session &operator=(const Session &) = delete;

    // Configures and starts the machine; returns once it runs or failed.
    bm_status_t start();
    void        pause();
    void        resume();
    void        reset();
    void        key(bm_key_code_t key, bool pressed);
    // Current persistent states, saved from the running machine.
    States capture();
    // Stops the machine and returns its final persistent states.
    States stop();

    bm_session_state_t state() const { return m_state.load(); }
    uint64_t           ticks() const { return m_ticks.load(); }
    // Emulated time as a percentage of wall-clock time; negative when idle.
    double realTimePercent() const { return m_realTime.load(); }

private:
    enum class CommandKind { Pause, Resume, Reset, Key, Capture, Stop };
    struct Command {
        CommandKind      kind;
        bm_input_event_t input {};
    };
    struct Machine;

    void   enqueue(const Command &command);
    void   run();
    bool   process(bm_session_t *session);
    States captureFrom(bm_session_t *session) const;
    void   publish(bm_session_t *session, bm_status_t status, bool render);

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
};

}

#endif
