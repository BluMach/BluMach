/*
 * BluMach bridge to the CoreMach engine: sound output.
 *
 * A CoreMach machine plays through its own OpenAL device and source, opened
 * by the session thread; the 86Box sound system is never started in a
 * CoreMach process. Builds without OpenAL have no sound for these machines.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_AUDIO_HPP
#define QT_COREMACH_AUDIO_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace CoreMachBridge {

// Mono 16-bit output, used from one thread only.
class AudioOutput final {
public:
    explicit AudioOutput(uint32_t sampleRate);
    ~AudioOutput();
    AudioOutput(const AudioOutput &)            = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    // Whether this build can play sound at all.
    static bool available();
    // Opens the default device; false when there is none.
    bool open();
    bool isOpen() const;
    // Queues samples. Playback starts once a few buffers are queued; when
    // more than the latency limit is waiting, the newest samples are dropped.
    void play(const int16_t *samples, size_t count);
    // Stops playback and forgets what is queued.
    void stop();
    void close();

private:
    struct Device;
    void queue(const int16_t *samples, size_t count);

    uint32_t                m_sampleRate;
    std::unique_ptr<Device> m_device;
    std::vector<int16_t>    m_pending;
};

}

#endif
