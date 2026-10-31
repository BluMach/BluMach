/*
 * BluMach bridge to the CoreMach engine: sound of a one-bit source.
 *
 * CoreMach machines report each change of what a buzzer plays, a drive
 * level or a square wave, at the emulated time it happens
 * (bm_audio_change_t). The buzzer turns those changes into samples, each the
 * average of the signal over its period, and removes the steady part of the
 * signal as a speaker does, so a level held high is silent. A tone above
 * half the sample rate cannot be sampled and plays as its mean: silence.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_BUZZER_HPP
#define QT_COREMACH_BUZZER_HPP

#include <blumach/engine/host.h>

#include <cstdint>
#include <deque>
#include <vector>

namespace CoreMachBridge {

class Buzzer final {
public:
    static constexpr uint32_t sampleRate = 48000;
    // Peak of a full-swing square wave, in 16-bit sample units.
    static constexpr double amplitude = 8000.0;

    explicit Buzzer(uint64_t ticksPerSecond);

    // Changes arrive in emulated time order; one earlier than the samples
    // already rendered takes effect at the next sample.
    void change(const bm_audio_change_t &change);
    // Appends the samples that end at or before emulated time `until`.
    void render(uint64_t until, std::vector<int16_t> &samples);
    // Starts rendering at `time` with nothing rendered before it.
    void restart(uint64_t time);

private:
    struct Source {
        double   level  = 0.0; // without a tone: 0, 1, or 0.5 when inaudible
        double   period = 0.0; // ticks of the square wave, 0 without a tone
        uint64_t start  = 0;   // the wave starts high here
    };
    struct Change {
        uint64_t time;
        Source   source;
    };

    // Integral of the source's signal over [from, to), in ticks.
    static double integral(const Source &source, double from, double to);

    double             m_ticksPerSample;
    double             m_filter; // high-pass coefficient
    std::deque<Change> m_changes;
    Source             m_source;
    double             m_position = 0.0; // start of the next sample, ticks
    double             m_lastInput  = 0.0;
    double             m_lastOutput = 0.0;
};

}

#endif
