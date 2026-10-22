/*
 * BluMach bridge to the CoreMach engine: a running machine session.
 *
 * The M10 configuration below follows CoreMach's own M10 frontend adapter
 * (apps/frontend/src/m10_adapter.c): the clock starts at host local time
 * and keeps running while the machine is off, or starts at a fixed,
 * reproducible time.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_session.hpp"

#include <blumach/systems/olivetti_m10.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>

namespace CoreMachBridge {
namespace {
    constexpr uint64_t nanosecondsPerSecond = UINT64_C(1000000000);
    // Short emulation slices keep queued input from waiting behind catch-up.
    constexpr uint64_t chunksPerSecond = 200;
    // Each key transition gets 2 ms of guest time so a burst of Qt events
    // reaches the keyboard scan in order (as CoreMach's portable launcher).
    constexpr uint64_t keyTransitionsPerSecond = 500;
    // Fall behind by at most 100 ms; older debt is dropped, not replayed.
    constexpr uint64_t maximumLagPerSecond = 10;
    constexpr uint64_t framesPerSecond     = 60;

    uint64_t now()
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                         std::chrono::steady_clock::now().time_since_epoch())
                                         .count());
    }

    // Ticks of a rate covering a duration in nanoseconds, without overflow.
    uint64_t ticksIn(uint64_t nanoseconds, uint64_t ticksPerSecond)
    {
        const uint64_t seconds = nanoseconds / nanosecondsPerSecond;
        const uint64_t rest    = nanoseconds % nanosecondsPerSecond;
        return seconds * ticksPerSecond
            + static_cast<uint64_t>((static_cast<long double>(rest) * ticksPerSecond) / nanosecondsPerSecond);
    }

    uint64_t interval(uint64_t ticksPerSecond, uint64_t perSecond)
    {
        return std::max<uint64_t>(1, ticksPerSecond / perSecond);
    }

    void *hostAllocate(void *, size_t size) { return std::malloc(size); }
    void  hostRelease(void *, void *allocation) { std::free(allocation); }
    bm_tick_t hostTime(void *) { return now(); }
    void hostLog(void *, bm_log_level_t level, const char *message)
    {
        if (level >= BM_LOG_WARNING && message)
            std::fprintf(stderr, "CoreMach: %s\n", message);
    }

    int64_t hostWallClock(void *) { return static_cast<int64_t>(std::time(nullptr)); }

    bm_upd1990ac_time_t hostLocalTime()
    {
        const std::time_t   current = std::time(nullptr);
        std::tm             local {};
        bm_upd1990ac_time_t result {};
        result.day   = 1;
        result.month = 1;
#ifdef _WIN32
        if (localtime_s(&local, &current) != 0)
            return result;
#else
        if (localtime_r(&current, &local) == nullptr)
            return result;
#endif
        result.second  = static_cast<uint8_t>(std::min(local.tm_sec, 59));
        result.minute  = static_cast<uint8_t>(local.tm_min);
        result.hour    = static_cast<uint8_t>(local.tm_hour);
        result.day     = static_cast<uint8_t>(local.tm_mday);
        result.weekday = static_cast<uint8_t>(local.tm_wday); // 0 = Sunday, as the ROM
        result.month   = static_cast<uint8_t>(local.tm_mon + 1);
        return result;
    }
}

struct Session::Machine {
    QByteArray          firmware;
    QByteArray          ram;
    QByteArray          rtc;
    bm_m10_config_t     m10 {};
    bm_machine_config_t config {};
    bm_host_services_t  host {};
    uint64_t            ticksPerSecond = 0;
    bool                valid          = false;
};

Session::Session(const Profile &profile, const QByteArray &firmware, const States &states,
                 FrameHandler handler)
    : m_machine(std::make_unique<Machine>())
    , m_handler(std::move(handler))
{
    Machine &machine = *m_machine;
    machine.host     = { nullptr, hostAllocate, hostRelease, hostTime, hostLog, nullptr, 0 };
    machine.firmware = firmware;

    if (profile.system && QLatin1String(profile.system->id) == QLatin1String("olivetti-m10")) {
        machine.ram = states.value(QStringLiteral("ram"));
        machine.rtc = states.value(QStringLiteral("rtc"));

        bm_m10_config_t &m10 = machine.m10;
        m10.firmware = { "firmware", reinterpret_cast<const uint8_t *>(machine.firmware.constData()),
                         static_cast<size_t>(machine.firmware.size()), nullptr };
        m10.ram_kib  = profile.value("ram_kib");
        m10.language = static_cast<bm_m10_language_t>(profile.value("language"));
        if (machine.ram.size() == BM_M10_RAM_STATE_SIZE) {
            m10.ram_state      = reinterpret_cast<const uint8_t *>(machine.ram.constData());
            m10.ram_state_size = BM_M10_RAM_STATE_SIZE;
        }
        if (machine.rtc.size() == BM_M10_RTC_STATE_SIZE) {
            m10.rtc_state      = reinterpret_cast<const uint8_t *>(machine.rtc.constData());
            m10.rtc_state_size = BM_M10_RTC_STATE_SIZE;
        }
        if (profile.value("rtc_time") == 0) {
            // The clock starts at host time and runs on while switched off.
            m10.rtc_initial_time = hostLocalTime();
            m10.wall_clock       = hostWallClock;
        } else {
            // Reproducible: Sunday 1 January 00:00:00, stopped while off.
            m10.rtc_initial_time.day   = 1;
            m10.rtc_initial_time.month = 1;
        }
        machine.config = bm_m10_machine_config(&m10);
        machine.valid  = true;
    }
    if (machine.valid)
        machine.ticksPerSecond = machine.config.definition->scheduler_ticks_per_second;
}

Session::~Session()
{
    if (m_thread.joinable())
        stop();
}

bm_status_t
Session::start()
{
    if (!m_machine->valid || m_machine->ticksPerSecond == 0)
        return BM_STATUS_INVALID_ARGUMENT;
    if (m_thread.joinable())
        return BM_STATUS_INVALID_STATE;
    m_thread = std::thread([this] { run(); });
    std::unique_lock<std::mutex> lock(m_mutex);
    m_done.wait(lock, [this] { return m_started; });
    return m_startStatus;
}

void Session::pause() { enqueue({ CommandKind::Pause }); }
void Session::resume() { enqueue({ CommandKind::Resume }); }
void Session::reset() { enqueue({ CommandKind::Reset }); }

void
Session::key(bm_key_code_t key, bool pressed)
{
    Command command { CommandKind::Key };
    command.input.kind    = BM_INPUT_KEY;
    command.input.key     = key;
    command.input.pressed = pressed ? 1 : 0;
    enqueue(command);
}

States
Session::capture()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_thread.joinable() || m_finished)
        return m_captured;
    const uint64_t request = ++m_captureRequests;
    m_commands.push_back({ CommandKind::Capture });
    m_wake.notify_all();
    m_done.wait(lock, [this, request] { return m_captureReplies >= request || m_finished; });
    return m_captured;
}

States
Session::stop()
{
    if (m_thread.joinable()) {
        enqueue({ CommandKind::Stop });
        m_thread.join();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_captured;
}

void
Session::enqueue(const Command &command)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_commands.push_back(command);
    }
    m_wake.notify_all();
}

States
Session::captureFrom(bm_session_t *session) const
{
    States states;
    const auto current = bm_session_state(session);
    if (current != BM_SESSION_RUNNING && current != BM_SESSION_PAUSED)
        return states;
    const auto *system = knownSystem(QString::fromLatin1(m_machine->config.definition->id));
    if (!system)
        return states;
    for (size_t index = 0; index < system->stateCount; ++index) {
        const char *role = system->states[index].role;
        size_t      size = 0;
        if (bm_session_persistent_state_size(session, role, &size) != BM_STATUS_OK || size == 0)
            continue;
        QByteArray data(static_cast<qsizetype>(size), '\0');
        if (bm_session_save_persistent_state(session, role, reinterpret_cast<uint8_t *>(data.data()), size)
            == BM_STATUS_OK)
            states.insert(QString::fromLatin1(role), data);
    }
    return states;
}

void
Session::publish(bm_session_t *session, bm_status_t status, bool render)
{
    publish(session, status, render, Notice());
}

void
Session::publish(bm_session_t *session, bm_status_t status, bool render, const Notice &notice)
{
    Frame frame;
    frame.status = status;
    frame.notice = notice;
    frame.state  = session ? bm_session_state(session) : BM_SESSION_NEW;
    m_state.store(frame.state);
    if (session)
        m_ticks.store(bm_session_time(session));
    if (render && session && status == BM_STATUS_OK) {
        bm_video_geometry_t geometry {};
        if (bm_session_video_geometry(session, &geometry) == BM_STATUS_OK && geometry.width > 0
            && geometry.height > 0 && geometry.width <= 4096 && geometry.height <= 4096) {
            QImage image(static_cast<int>(geometry.width), static_cast<int>(geometry.height), QImage::Format_RGB32);
            image.fill(0U);
            bm_video_framebuffer_t framebuffer { reinterpret_cast<uint32_t *>(image.bits()),
                                                 static_cast<size_t>(image.sizeInBytes()) / sizeof(uint32_t),
                                                 static_cast<uint32_t>(image.bytesPerLine() / sizeof(uint32_t)),
                                                 geometry };
            // A frame that cannot be drawn is skipped; emulation goes on.
            const bm_status_t video = bm_session_render_video(session, &framebuffer);
            if (video == BM_STATUS_OK)
                frame.image = image;
            else if (frame.notice.kind == Notice::Kind::None)
                frame.notice = { Notice::Kind::VideoFailed, video, {} };
        }
    }
    if (m_handler)
        m_handler(frame);
}

bool
Session::stopAfterFailure(bm_session_t *session, bm_status_t status)
{
    // The engine cannot go on after a failed run: keep the last states and
    // stop. The frame published here carries the failure to the window.
    std::fprintf(stderr, "BluMach: CoreMach session stopped with status %d\n", static_cast<int>(status));
    States states = captureFrom(session);
    (void) bm_session_stop(session);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!states.isEmpty())
            m_captured = states;
    }
    publish(session, status, false);
    return false;
}

bool
Session::process(bm_session_t *session)
{
    std::deque<Command> commands;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        commands.swap(m_commands);
    }
    const uint64_t tps = m_machine->ticksPerSecond;
    for (const auto &command : commands) {
        // Only a failed emulation run stops the session. A rejected command
        // or key is reported as a notice and the machine keeps running.
        bm_status_t status = BM_STATUS_OK;
        Notice      notice;
        switch (command.kind) {
            case CommandKind::Pause:
                if (bm_session_state(session) == BM_SESSION_RUNNING)
                    status = bm_session_pause(session);
                break;
            case CommandKind::Resume:
                if (bm_session_state(session) == BM_SESSION_PAUSED)
                    status = bm_session_resume(session);
                m_paceOrigin = now();
                m_paceTicks  = 0;
                break;
            case CommandKind::Reset:
                status       = bm_session_reset(session);
                m_paceOrigin = now();
                m_paceTicks  = 0;
                break;
            case CommandKind::Key: {
                const bm_status_t input = bm_session_send_input(session, &command.input);
                if (input == BM_STATUS_UNSUPPORTED)
                    notice = { Notice::Kind::KeyWithoutPosition, input, command.input.key };
                else if (input != BM_STATUS_OK)
                    notice = { Notice::Kind::InputRejected, input, command.input.key };
                else if (bm_session_state(session) == BM_SESSION_RUNNING) {
                    const uint64_t transition = interval(tps, keyTransitionsPerSecond);
                    status = bm_session_run_for(session, transition);
                    m_paceTicks += transition;
                }
                break;
            }
            case CommandKind::Capture: {
                States states = captureFrom(session);
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!states.isEmpty())
                    m_captured = states;
                ++m_captureReplies;
                m_done.notify_all();
                break;
            }
            case CommandKind::Stop: {
                States states = captureFrom(session);
                (void) bm_session_stop(session);
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!states.isEmpty())
                    m_captured = states;
                return false;
            }
        }
        if ((command.kind == CommandKind::Pause || command.kind == CommandKind::Resume
             || command.kind == CommandKind::Reset)
            && status != BM_STATUS_OK) {
            notice = { Notice::Kind::CommandFailed, status, {} };
            status = BM_STATUS_OK;
        }
        if (status != BM_STATUS_OK) {
            (void) stopAfterFailure(session, status);
            continue;
        }
        if (command.kind != CommandKind::Capture)
            publish(session, status, false, notice);
    }
    return true;
}

void
Session::run()
{
    bm_session_t *session = nullptr;
    bm_status_t   status  = bm_session_create(&m_machine->host, &session);
    if (status == BM_STATUS_OK)
        status = bm_session_configure(session, &m_machine->config);
    if (status == BM_STATUS_OK)
        status = bm_session_start(session);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_started     = true;
        m_startStatus = status;
        if (status != BM_STATUS_OK)
            m_finished = true;
    }
    m_done.notify_all();
    publish(session, status, false);
    if (status != BM_STATUS_OK) {
        if (session)
            bm_session_destroy(session);
        return;
    }

    const uint64_t tps     = m_machine->ticksPerSecond;
    const uint64_t chunk   = interval(tps, chunksPerSecond);
    const uint64_t maxLag  = interval(tps, maximumLagPerSecond);
    const uint64_t frameNs = nanosecondsPerSecond / framesPerSecond;
    m_paceOrigin = m_lastFrame = m_speedOrigin = now();
    m_paceTicks = m_speedTicks = 0;

    while (process(session)) {
        const bool running = bm_session_state(session) == BM_SESSION_RUNNING;
        if (running) {
            const uint64_t current = now();
            const uint64_t target  = ticksIn(current - m_paceOrigin, tps);
            if (target > m_paceTicks + maxLag)
                m_paceTicks = target - maxLag;
            const uint64_t due = std::min(target > m_paceTicks ? target - m_paceTicks : 0, chunk);
            if (due > 0) {
                status = bm_session_run_for(session, due);
                m_paceTicks += due;
            }
            if (status != BM_STATUS_OK) {
                (void) stopAfterFailure(session, status);
                status = BM_STATUS_OK;
            } else if (now() - m_lastFrame >= frameNs) {
                m_lastFrame = now();
                publish(session, BM_STATUS_OK, true);
            }
            const uint64_t elapsed = now() - m_speedOrigin;
            if (elapsed >= nanosecondsPerSecond) {
                const uint64_t ticks = bm_session_time(session);
                m_realTime.store(100.0 * static_cast<double>(ticks - m_speedTicks) / static_cast<double>(tps)
                                 / (static_cast<double>(elapsed) / nanosecondsPerSecond));
                m_speedOrigin = now();
                m_speedTicks  = ticks;
            }
        } else {
            m_realTime.store(-1.0);
            m_speedOrigin = now();
            m_speedTicks  = bm_session_time(session);
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_commands.empty())
            continue;
        if (bm_session_state(session) == BM_SESSION_RUNNING) {
            // Sleep until the next slice is due, or a command arrives.
            const uint64_t current = now();
            const uint64_t target  = ticksIn(current - m_paceOrigin, tps);
            if (target < m_paceTicks + chunk)
                m_wake.wait_for(lock, std::chrono::nanoseconds(std::min<uint64_t>(
                                          (m_paceTicks + chunk - target) * nanosecondsPerSecond / tps, frameNs)));
        } else
            m_wake.wait(lock, [this] { return !m_commands.empty(); });
    }

    bm_session_destroy(session);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_finished = true;
    }
    m_done.notify_all();
    m_state.store(BM_SESSION_STOPPED);
    m_realTime.store(-1.0);
}

}
