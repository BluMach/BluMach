/*
 * BluMach bridge to the CoreMach engine: cassette tapes as WAV files.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_tape.hpp"

#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace CoreMachBridge {
namespace {
    uint16_t le16(const char *data) { return qFromLittleEndian<uint16_t>(data); }
    uint32_t le32(const char *data) { return qFromLittleEndian<uint32_t>(data); }

    bool fail(QString *reason, const char *text)
    {
        if (reason)
            *reason = QString::fromLatin1(text);
        return false;
    }

    void append16(QByteArray &out, uint16_t value)
    {
        char bytes[2];
        qToLittleEndian(value, bytes);
        out.append(bytes, 2);
    }

    void append32(QByteArray &out, uint32_t value)
    {
        char bytes[4];
        qToLittleEndian(value, bytes);
        out.append(bytes, 4);
    }
}

bool
readWav(const QByteArray &file, Tape *tape, QString *reason)
{
    if (file.size() < 12 || std::memcmp(file.constData(), "RIFF", 4) != 0
        || std::memcmp(file.constData() + 8, "WAVE", 4) != 0)
        return fail(reason, "not a WAV file");
    uint16_t   format = 0, channels = 0, bits = 0;
    uint32_t   rate   = 0;
    const char *data  = nullptr;
    qsizetype   size  = 0;
    for (qsizetype at = 12; at + 8 <= file.size();) {
        const char     *chunk  = file.constData() + at;
        const qsizetype length = static_cast<qsizetype>(le32(chunk + 4));
        const qsizetype body   = at + 8;
        if (length < 0 || body + length > file.size()) {
            if (std::memcmp(chunk, "data", 4) != 0)
                return fail(reason, "truncated WAV file");
            data = file.constData() + body; // a truncated data chunk: keep what is there
            size = file.size() - body;
            break;
        }
        if (std::memcmp(chunk, "fmt ", 4) == 0 && length >= 16) {
            format   = le16(chunk + 8);
            channels = le16(chunk + 10);
            rate     = le32(chunk + 12);
            bits     = le16(chunk + 22);
            // WAVE_FORMAT_EXTENSIBLE: the format is the subformat GUID's first two bytes.
            if (format == 0xfffe && length >= 40)
                format = le16(chunk + 32);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            data = file.constData() + body;
            size = length;
        }
        at = body + length + (length & 1);
    }
    const bool integer  = format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
    const bool floating = format == 3 && bits == 32;
    if ((!integer && !floating) || channels == 0 || rate == 0)
        return fail(reason, "only PCM of 8, 16, 24 or 32 bits or 32-bit float");
    if (!data)
        return fail(reason, "no audio data");
    const int       width  = bits / 8;
    const qsizetype frame  = channels * width;
    const qsizetype frames = size / frame;
    tape->sampleRate       = rate;
    tape->samples.resize(static_cast<size_t>(frames));
    for (qsizetype index = 0; index < frames; ++index) {
        double sum = 0;
        for (int channel = 0; channel < channels; ++channel) {
            const char *sample = data + index * frame + channel * width;
            if (floating) {
                float value;
                const uint32_t raw = le32(sample);
                std::memcpy(&value, &raw, sizeof(value));
                sum += std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) * 32767.0 : 0.0;
            } else if (bits == 8)
                sum += (static_cast<int>(static_cast<uint8_t>(*sample)) - 128) * 256;
            else // the top 16 bits of a signed little-endian sample
                sum += static_cast<int16_t>(le16(sample + width - 2));
        }
        tape->samples[static_cast<size_t>(index)] = static_cast<int16_t>(sum / channels);
    }
    return true;
}

double
adjustVolume(Tape &tape)
{
    if (tape.samples.empty())
        return 1.0;
    int64_t sum = 0;
    for (const int16_t sample : tape.samples)
        sum += sample;
    const int        offset = static_cast<int>(sum / static_cast<int64_t>(tape.samples.size()));
    std::vector<int> levels(tape.samples.size());
    for (size_t index = 0; index < levels.size(); ++index)
        levels[index] = std::abs(tape.samples[index] - offset);
    const auto peak = levels.begin() + static_cast<std::ptrdiff_t>(levels.size() - 1 - levels.size() / 10000);
    std::nth_element(levels.begin(), peak, levels.end());
    constexpr int target = 16384;
    const double  gain   = (*peak == 0 || *peak >= target / 2) ? 1.0 : std::min(64.0, double(target) / *peak);
    for (int16_t &sample : tape.samples)
        sample = static_cast<int16_t>(std::clamp((sample - offset) * gain, -32768.0, 32767.0));
    return gain;
}

QByteArray
writeWav(const Tape &tape)
{
    const uint32_t bytes = static_cast<uint32_t>(tape.samples.size() * 2);
    QByteArray     out;
    out.reserve(static_cast<qsizetype>(44 + bytes));
    out.append("RIFF", 4);
    append32(out, 36 + bytes);
    out.append("WAVEfmt ", 8);
    append32(out, 16);
    append16(out, 1); // PCM
    append16(out, 1); // mono
    append32(out, tape.sampleRate);
    append32(out, tape.sampleRate * 2);
    append16(out, 2);
    append16(out, 16);
    out.append("data", 4);
    append32(out, bytes);
    for (const int16_t sample : tape.samples)
        append16(out, static_cast<uint16_t>(sample));
    return out;
}

TapeRecorder::TapeRecorder(uint64_t ticksPerSecond)
    : m_ticksPerSample(static_cast<double>(ticksPerSecond) / sampleRate)
{
}

void
TapeRecorder::fill(uint64_t until)
{
    // The tape moves only while the motor runs.
    if (!m_motor)
        return;
    const int16_t value = m_level ? amplitude : static_cast<int16_t>(-amplitude);
    while (m_next < static_cast<double>(until)) {
        m_samples.push_back(value);
        m_next += m_ticksPerSample;
    }
}

void
TapeRecorder::motor(bool on, uint64_t time)
{
    fill(time);
    if (on && !m_motor)
        m_next = static_cast<double>(time);
    m_motor = on;
}

void
TapeRecorder::record(bool level, uint64_t time)
{
    fill(time);
    m_level = level;
}

}
