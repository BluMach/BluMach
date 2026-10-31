/*
 * BluMach bridge to the CoreMach engine: sound of a one-bit source.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_buzzer.hpp"

#include <algorithm>
#include <cmath>

namespace CoreMachBridge {
namespace {
    // A speaker passes little below about 20 Hz.
    constexpr double highPassHz = 20.0;
    constexpr double pi         = 3.14159265358979323846;
}

Buzzer::Buzzer(uint64_t ticksPerSecond)
    : m_ticksPerSample(static_cast<double>(ticksPerSecond) / sampleRate)
{
    const double rc = 1.0 / (2.0 * pi * highPassHz);
    m_filter        = rc / (rc + 1.0 / sampleRate);
}

void
Buzzer::change(const bm_audio_change_t &change)
{
    Change next { change.time, {} };
    if (change.tone_numerator != 0 && change.tone_denominator != 0) {
        const double hz = static_cast<double>(change.tone_numerator) / static_cast<double>(change.tone_denominator);
        if (hz > sampleRate / 2.0)
            next.source.level = 0.5; // inaudible: only its mean, which the filter removes
        else {
            next.source.period = m_ticksPerSample * sampleRate / hz;
            next.source.start  = change.time;
        }
    } else
        next.source.level = change.level ? 1.0 : 0.0;
    m_changes.push_back(next);
}

double
Buzzer::integral(const Source &source, double from, double to)
{
    if (to <= from)
        return 0.0;
    if (source.period <= 0.0)
        return source.level * (to - from);
    // High for the first half of each period from the start of the wave.
    const auto high = [&source](double time) {
        const double offset = time - static_cast<double>(source.start);
        const double cycles = std::floor(offset / source.period);
        const double phase  = offset - cycles * source.period;
        return cycles * source.period / 2.0 + std::min(phase, source.period / 2.0);
    };
    return high(to) - high(from);
}

void
Buzzer::render(uint64_t until, std::vector<int16_t> &samples)
{
    const double end = static_cast<double>(until);
    while (m_position + m_ticksPerSample <= end) {
        const double sampleEnd = m_position + m_ticksPerSample;
        double       position  = m_position;
        double       sum       = 0.0;
        while (!m_changes.empty() && static_cast<double>(m_changes.front().time) < sampleEnd) {
            const double at = std::max(position, static_cast<double>(m_changes.front().time));
            sum += integral(m_source, position, at);
            position = at;
            m_source = m_changes.front().source;
            m_changes.pop_front();
        }
        sum += integral(m_source, position, sampleEnd);
        const double input = sum / m_ticksPerSample;
        m_lastOutput       = m_filter * (m_lastOutput + input - m_lastInput);
        m_lastInput        = input;
        const double value = std::clamp(m_lastOutput * 2.0 * amplitude, -32767.0, 32767.0);
        samples.push_back(static_cast<int16_t>(std::lround(value)));
        m_position = sampleEnd;
    }
}

void
Buzzer::restart(uint64_t time)
{
    while (!m_changes.empty() && m_changes.front().time <= time) {
        m_source = m_changes.front().source;
        m_changes.pop_front();
    }
    m_position = static_cast<double>(time);
}

}
