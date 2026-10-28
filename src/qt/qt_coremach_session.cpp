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
#include "qt_coremach_buzzer.hpp"

#include <blumach/systems/olivetti_m10.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iterator>
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

    // Called on the session thread, inside bm_session_run_for().
    void hostAudioChange(void *context, const bm_audio_change_t *change)
    {
        if (change && change->size >= sizeof(bm_audio_change_t) && change->channel == 0)
            static_cast<Buzzer *>(context)->change(*change);
    }

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

    std::unique_ptr<Buzzer>   buzzer;
    bm_host_audio_services_t  audio {};
    bm_host_serial_services_t   serial {};
    bm_host_cassette_services_t  cassette {};
    bm_host_parallel_services_t  printer {};
    bm_host_tolerance_services_t tolerance {};
    bm_host_capability_t         capabilities[5] {};
};

Session::Session(const Profile &profile, const QByteArray &firmware, const States &states,
                 FrameHandler handler, bool tolerant)
    : m_machine(std::make_unique<Machine>())
    , m_handler(std::move(handler))
    , m_audio(Buzzer::sampleRate)
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
    if (machine.valid && machine.ticksPerSecond > 0) {
        // The machine reports its buzzer to the host; silent until setSound().
        machine.buzzer          = std::make_unique<Buzzer>(machine.ticksPerSecond);
        machine.audio           = { hostAudioChange, machine.buzzer.get() };
        machine.serial          = { serialTransmitted, this };
        machine.cassette        = { tapeMotor, tapeRecord, this };
        // A printer that is never busy and always on line.
        machine.printer         = { printerWrite, nullptr, this };
        machine.tolerance       = { tolerated, this };
        machine.capabilities[0] = { BM_HOST_CAPABILITY_AUDIO_OUTPUT, BM_HOST_AUDIO_VERSION, sizeof(machine.audio),
                                    &machine.audio };
        machine.capabilities[1] = { BM_HOST_CAPABILITY_SERIAL, BM_HOST_SERIAL_VERSION, sizeof(machine.serial),
                                    &machine.serial };
        machine.capabilities[2] = { BM_HOST_CAPABILITY_CASSETTE, BM_HOST_CASSETTE_VERSION, sizeof(machine.cassette),
                                    &machine.cassette };
        machine.capabilities[3] = { BM_HOST_CAPABILITY_PARALLEL, BM_HOST_PARALLEL_VERSION, sizeof(machine.printer),
                                    &machine.printer };
        // Tolerant (last, so a strict session leaves it out): unmodelled ports and memory, extended 8085 instructions
        // and unknown device commands fall back as documented by CoreMach
        // and are reported, instead of stopping the machine.
        machine.capabilities[4] = { BM_HOST_CAPABILITY_TOLERANCE, BM_HOST_TOLERANCE_VERSION,
                                    sizeof(machine.tolerance), &machine.tolerance };
        machine.host.capabilities     = machine.capabilities;
        machine.host.capability_count = std::size(machine.capabilities) - (tolerant ? 0 : 1);
    }
}

QString
toleratedKind(const Session::Tolerated &event)
{
    switch (event.kind) {
        case BM_TOLERANCE_UNMODELLED_IO_READ:
            return QStringLiteral("io_read");
        case BM_TOLERANCE_UNMODELLED_IO_WRITE:
            return QStringLiteral("io_write");
        case BM_TOLERANCE_UNMODELLED_MEMORY_READ:
            return QStringLiteral("memory_read");
        case BM_TOLERANCE_UNMODELLED_MEMORY_WRITE:
            return QStringLiteral("memory_write");
        case BM_TOLERANCE_UNDOCUMENTED_INSTRUCTION:
            return QStringLiteral("undocumented_instruction");
        case BM_TOLERANCE_UNKNOWN_DEVICE_COMMAND:
            return QStringLiteral("device_command");
    }
    return QStringLiteral("tolerated");
}

QString
toleratedDetail(const Session::Tolerated &event)
{
    const auto hex = [](uint64_t value, int digits) {
        return QStringLiteral("%1").arg(value, digits, 16, QLatin1Char('0')).toUpper() + QLatin1Char('h');
    };
    QString detail;
    switch (event.kind) {
        case BM_TOLERANCE_UNMODELLED_IO_READ:
        case BM_TOLERANCE_UNMODELLED_IO_WRITE:
            detail = hex(event.address, 2);
            break;
        case BM_TOLERANCE_UNMODELLED_MEMORY_READ:
        case BM_TOLERANCE_UNMODELLED_MEMORY_WRITE:
            detail = hex(event.address, 4);
            break;
        case BM_TOLERANCE_UNDOCUMENTED_INSTRUCTION:
            detail = hex(event.opcode, 2);
            break;
        case BM_TOLERANCE_UNKNOWN_DEVICE_COMMAND:
            detail = event.device + QLatin1Char(' ') + hex(event.value, 2);
            break;
    }
    if (event.flags & BM_TOLERANCE_FLAG_HAS_PC)
        detail += QStringLiteral(" at PC %1 (%2)").arg(hex(event.pc, 4), hex(event.opcode, 2));
    return detail;
}

QByteArray
serialText(const QByteArray &file)
{
    QByteArray text;
    text.reserve(file.size() + file.count('\n') + 1);
    for (qsizetype index = 0; index < file.size(); ++index) {
        const char c = file.at(index);
        if (c == '\x1a')
            break; // the end of the text
        if (c == '\r' || c == '\n') {
            text.append("\r\n");
            if (c == '\r' && index + 1 < file.size() && file.at(index + 1) == '\n')
                ++index;
        } else
            text.append(c);
    }
    text.append('\x1a');
    return text;
}

QByteArray
PrintoutText::convert(const QByteArray &printed)
{
    QByteArray text;
    text.reserve(printed.size() + printed.count('\r'));
    for (const char c : printed) {
        if (c == '\n' && m_afterCr) {
            m_afterCr = false;
            continue;
        }
        m_afterCr = c == '\r';
        if (m_afterCr)
            text.append("\r\n");
        else
            text.append(c);
    }
    return text;
}

QString
printoutFileName(const QDateTime &time)
{
    return QStringLiteral("print-%1.txt").arg(time.toString(QStringLiteral("yyyyMMdd-HHmmss")));
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

void
Session::setSound(bool on)
{
    Command command { CommandKind::Sound };
    command.on = on;
    enqueue(command);
}

void
Session::sendSerial(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    Command command { CommandKind::SerialSend };
    command.data = data;
    m_serialPending.fetch_add(data.size());
    enqueue(command);
}

void Session::cancelSerial() { enqueue({ CommandKind::SerialCancel }); }

void
Session::serialTransmitted(void *context, uint32_t port, uint8_t data, bm_tick_t)
{
    // Called on the session thread, inside bm_session_run_for().
    auto *session = static_cast<Session *>(context);
    if (port != 0)
        return;
    if (data == 0x13) // XOFF
        session->m_xoff = true;
    else if (data == 0x11) // XON
        session->m_xoff = false;
    session->m_serialIn.append(static_cast<char>(data));
}

void
Session::printerWrite(void *context, uint32_t port, uint8_t data, bm_tick_t)
{
    // Called on the session thread, inside bm_session_run_for().
    auto *session = static_cast<Session *>(context);
    if (port == 0)
        session->m_printed.append(static_cast<char>(data));
}

void
Session::feedSerial(bm_session_t *session)
{
    if (!m_serialOut.isEmpty() && !m_xoff) {
        size_t            accepted = 0;
        const bm_status_t status   = bm_session_serial_receive(session, 0,
                                                               reinterpret_cast<const uint8_t *>(m_serialOut.constData()),
                                                               static_cast<size_t>(m_serialOut.size()), &accepted);
        if (status == BM_STATUS_OK)
            m_serialOut.remove(0, static_cast<qsizetype>(accepted));
        else
            m_serialOut.clear(); // no serial port to take them
    }
    m_serialPending.store(m_serialOut.size());
}

void
Session::loadTape(const Tape &tape)
{
    Command command { CommandKind::TapeLoad };
    command.tape = tape;
    enqueue(command);
}

void Session::rewindTape() { enqueue({ CommandKind::TapeRewind }); }
void Session::ejectTape() { enqueue({ CommandKind::TapeEject }); }

void
Session::startRecording()
{
    m_recording.store(true);
    enqueue({ CommandKind::RecordStart });
}

Tape
Session::stopRecording()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_thread.joinable() && !m_finished) {
        const uint64_t request = ++m_recordRequests;
        m_commands.push_back({ CommandKind::RecordStop });
        m_wake.notify_all();
        m_done.wait(lock, [this, request] { return m_recordReplies >= request || m_finished; });
    }
    m_recording.store(false);
    Tape recorded;
    std::swap(recorded, m_recorded);
    return recorded;
}

Session::TapeStatus
Session::tapeStatus() const
{
    TapeStatus status;
    status.loaded    = m_tapeStatusLoaded.load();
    status.motor     = m_motor.load();
    status.recording = m_recording.load();
    status.position  = m_tapePosition.load();
    status.length    = m_tapeLength.load();
    status.rate      = m_tapeRate.load();
    status.recorded  = m_recordedSamples.load();
    return status;
}

void
Session::tolerated(void *context, const bm_tolerance_event_t *event)
{
    // Called on the session thread, inside bm_session_run_for().
    if (!event || event->size < sizeof(bm_tolerance_event_t))
        return;
    Tolerated report;
    report.kind    = event->kind;
    report.flags   = event->flags;
    report.device  = QString::fromUtf8(event->device ? event->device : "");
    report.note    = QString::fromUtf8(event->note ? event->note : "");
    report.pc      = event->pc;
    report.opcode  = event->opcode;
    report.address = event->address;
    report.value   = event->value;
    static_cast<Session *>(context)->m_tolerated.push_back(report);
}

void
Session::tapeMotor(void *context, uint32_t port, int on, bm_tick_t time)
{
    // Called on the session thread, inside bm_session_run_for() or reset.
    auto *session = static_cast<Session *>(context);
    if (port != 0)
        return;
    session->m_motor.store(on != 0);
    if (session->m_recorder)
        session->m_recorder->motor(on != 0, time);
}

void
Session::tapeRecord(void *context, uint32_t port, int level, bm_tick_t time)
{
    auto *session = static_cast<Session *>(context);
    if (port == 0 && session->m_recorder)
        session->m_recorder->record(level != 0, time);
}

void
Session::insertTape(bm_session_t *session, size_t position)
{
    if (!m_tapeLoaded) {
        (void) bm_session_cassette_load(session, 0, nullptr);
        return;
    }
    const bm_cassette_tape_t medium { m_tape.samples.data(), m_tape.samples.size(), m_tape.sampleRate,
                                      std::min(position, m_tape.samples.size()) };
    (void) bm_session_cassette_load(session, 0, &medium);
}

void
Session::updateTapeStatus(bm_session_t *session)
{
    size_t position = 0;
    m_tapeStatusLoaded.store(m_tapeLoaded);
    if (m_tapeLoaded && bm_session_cassette_position(session, 0, &position) == BM_STATUS_OK)
        m_tapePosition.store(position);
    m_tapeLength.store(m_tapeLoaded ? m_tape.samples.size() : 0);
    m_tapeRate.store(m_tapeLoaded ? m_tape.sampleRate : 0);
    m_recordedSamples.store(m_recorder ? m_recorder->samples().size() : 0);
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
    frame.serial.swap(m_serialIn);
    frame.printed.swap(m_printed);
    frame.tolerated.swap(m_tolerated);
    bm_keyboard_led_state_t leds {};
    if (session && bm_session_keyboard_leds(session, &leds) == BM_STATUS_OK)
        frame.keyboardLeds = leds.indicators;
    uint64_t poweredOff = 0;
    if (session && bm_session_inspect_machine(session, "powered_off", &poweredOff) == BM_STATUS_OK)
        frame.poweredOff = poweredOff != 0;
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
    m_audio.close();
    return false;
}

void
Session::playSound(bm_session_t *session)
{
    if (!m_machine->buzzer)
        return;
    m_samples.clear();
    m_machine->buzzer->render(bm_session_time(session), m_samples);
    if (!m_samples.empty())
        m_audio.play(m_samples.data(), m_samples.size());
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
                m_audio.stop();
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
                m_serialOut.clear();
                m_serialPending.store(0);
                m_xoff = false;
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
            case CommandKind::Sound:
                if (!command.on)
                    m_audio.close();
                else if (m_machine->buzzer && !m_audio.open())
                    notice = { Notice::Kind::SoundUnavailable, BM_STATUS_UNSUPPORTED, {} };
                break;
            case CommandKind::SerialSend:
                m_serialOut.append(command.data);
                break;
            case CommandKind::SerialCancel:
                m_serialOut.clear();
                m_serialPending.store(0);
                break;
            case CommandKind::TapeLoad:
                m_tape       = command.tape;
                m_tapeLoaded = command.tape.sampleRate != 0;
                insertTape(session, 0);
                break;
            case CommandKind::TapeRewind:
                insertTape(session, 0);
                break;
            case CommandKind::TapeEject:
                m_tapeLoaded = false;
                insertTape(session, 0);
                m_tape = {};
                break;
            case CommandKind::RecordStart:
                if (!m_recorder) {
                    m_recorder = std::make_unique<TapeRecorder>(m_machine->ticksPerSecond);
                    // Recording starts at once if the motor already runs.
                    if (m_motor.load())
                        m_recorder->motor(true, bm_session_time(session));
                }
                break;
            case CommandKind::RecordStop: {
                if (m_recorder)
                    m_recorder->motor(false, bm_session_time(session));
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_recorder)
                    m_recorded = { m_recorder->samples(), TapeRecorder::sampleRate };
                m_recorder.reset();
                m_recordedSamples.store(0);
                ++m_recordReplies;
                m_done.notify_all();
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
                if (m_recorder)
                    m_recorder->motor(false, bm_session_time(session));
                (void) bm_session_stop(session);
                m_audio.close();
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!states.isEmpty())
                    m_captured = states;
                // A recording still running is kept for stopRecording().
                if (m_recorder)
                    m_recorded = { m_recorder->samples(), TapeRecorder::sampleRate };
                m_recorder.reset();
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
        // Commands that change nothing on screen publish only a notice.
        const bool quiet = command.kind != CommandKind::Pause && command.kind != CommandKind::Resume
            && command.kind != CommandKind::Reset && command.kind != CommandKind::Key;
        if (!quiet || notice.kind != Notice::Kind::None)
            publish(session, status, false, notice);
    }
    if (!commands.empty())
        updateTapeStatus(session);
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
    if (m_machine->buzzer)
        m_machine->buzzer->restart(bm_session_time(session));

    while (process(session)) {
        const bool running = bm_session_state(session) == BM_SESSION_RUNNING;
        if (running) {
            const uint64_t current = now();
            const uint64_t target  = ticksIn(current - m_paceOrigin, tps);
            if (target > m_paceTicks + maxLag)
                m_paceTicks = target - maxLag;
            const uint64_t due = std::min(target > m_paceTicks ? target - m_paceTicks : 0, chunk);
            feedSerial(session);
            if (due > 0) {
                status = bm_session_run_for(session, due);
                m_paceTicks += due;
                updateTapeStatus(session);
            }
            if (status == BM_STATUS_OK)
                playSound(session);
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

    m_audio.close();
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
