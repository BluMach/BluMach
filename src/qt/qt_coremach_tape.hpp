/*
 * BluMach bridge to the CoreMach engine: cassette tapes as WAV files.
 *
 * Playback tapes are mono 16-bit PCM at the file's own rate; CoreMach plays
 * them while the machine's recorder motor runs. Recordings keep the signal
 * the machine sends to MIC only while the motor runs, as a real tape would.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_TAPE_HPP
#define QT_COREMACH_TAPE_HPP

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

namespace CoreMachBridge {

struct Tape {
    std::vector<int16_t> samples; // mono
    uint32_t             sampleRate = 0;
};

// Reads a PCM WAV file (8 or 16 bits, mono or stereo, any rate); stereo is
// mixed to mono. Returns false with a short reason for anything else.
bool readWav(const QByteArray &file, Tape *tape, QString *reason = nullptr);
// A mono 16-bit PCM WAV file.
QByteArray writeWav(const Tape &tape);

// Turns the cassette reports of a machine into a recording.
class TapeRecorder final {
public:
    static constexpr uint32_t sampleRate = 48000;
    // Peak of the recorded square wave.
    static constexpr int16_t amplitude = 16000;

    explicit TapeRecorder(uint64_t ticksPerSecond);

    // Reports in emulated time order.
    void motor(bool on, uint64_t time);
    void record(bool level, uint64_t time);
    // The recording so far, up to the last report; it keeps recording.
    const std::vector<int16_t> &samples() const { return m_samples; }

private:
    void fill(uint64_t until);

    double               m_ticksPerSample;
    std::vector<int16_t> m_samples;
    bool                 m_motor = false;
    bool                 m_level = false;
    double               m_next  = 0.0; // time of the next sample, ticks
};

}

#endif
