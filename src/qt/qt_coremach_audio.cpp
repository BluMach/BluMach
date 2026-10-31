/*
 * BluMach bridge to the CoreMach engine: sound output.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_audio.hpp"

#ifdef BLUMACH_COREMACH_OPENAL
// Linked as 86Box links it (src/sound/openal.c).
#    undef AL_API
#    undef ALC_API
#    define AL_LIBTYPE_STATIC
#    define ALC_LIBTYPE_STATIC
#    include <AL/al.h>
#    include <AL/alc.h>
#endif

#include <algorithm>

namespace CoreMachBridge {
namespace {
    // 10 ms buffers; play after 40 ms are queued and keep at most 120 ms.
    constexpr uint32_t buffersPerSecond = 100;
    constexpr int      startBuffers     = 4;
    constexpr int      maximumBuffers   = 12;
}

#ifdef BLUMACH_COREMACH_OPENAL
struct AudioOutput::Device {
    ALCdevice          *device  = nullptr;
    ALCcontext         *context = nullptr;
    ALuint              source  = 0;
    std::vector<ALuint> buffers; // every buffer created
    std::vector<ALuint> free;    // created and not queued
};

bool
AudioOutput::available()
{
    return true;
}

bool
AudioOutput::open()
{
    if (m_device)
        return true;
    auto device    = std::make_unique<Device>();
    device->device = alcOpenDevice(nullptr);
    if (!device->device)
        return false;
    device->context = alcCreateContext(device->device, nullptr);
    if (!device->context || !alcMakeContextCurrent(device->context)) {
        if (device->context)
            alcDestroyContext(device->context);
        alcCloseDevice(device->device);
        return false;
    }
    alGetError();
    alGenSources(1, &device->source);
    if (alGetError() != AL_NO_ERROR) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(device->context);
        alcCloseDevice(device->device);
        return false;
    }
    alSourcei(device->source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(device->source, AL_ROLLOFF_FACTOR, 0.0f);
    m_device = std::move(device);
    return true;
}

void
AudioOutput::queue(const int16_t *samples, size_t count)
{
    Device &device    = *m_device;
    ALint   processed = 0;
    alGetSourcei(device.source, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(device.source, 1, &buffer);
        device.free.push_back(buffer);
    }
    ALint queued = 0;
    alGetSourcei(device.source, AL_BUFFERS_QUEUED, &queued);
    if (queued >= maximumBuffers)
        return; // ahead of the device: drop rather than add latency
    if (device.free.empty()) {
        ALuint buffer = 0;
        alGenBuffers(1, &buffer);
        if (alGetError() != AL_NO_ERROR)
            return;
        device.buffers.push_back(buffer);
        device.free.push_back(buffer);
    }
    const ALuint buffer = device.free.back();
    device.free.pop_back();
    alBufferData(buffer, AL_FORMAT_MONO16, samples, static_cast<ALsizei>(count * sizeof(int16_t)),
                 static_cast<ALsizei>(m_sampleRate));
    alSourceQueueBuffers(device.source, 1, &buffer);
    ++queued;
    ALint state = AL_STOPPED;
    alGetSourcei(device.source, AL_SOURCE_STATE, &state);
    // Start, or restart after running dry, once enough is queued.
    if (state != AL_PLAYING && queued >= startBuffers)
        alSourcePlay(device.source);
}

void
AudioOutput::stop()
{
    m_pending.clear();
    if (!m_device)
        return;
    Device &device = *m_device;
    alSourceStop(device.source);
    alSourcei(device.source, AL_BUFFER, 0); // unqueues everything
    device.free = device.buffers;
}

void
AudioOutput::close()
{
    if (!m_device)
        return;
    stop();
    Device &device = *m_device;
    alDeleteSources(1, &device.source);
    if (!device.buffers.empty())
        alDeleteBuffers(static_cast<ALsizei>(device.buffers.size()), device.buffers.data());
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(device.context);
    alcCloseDevice(device.device);
    m_device.reset();
}
#else
struct AudioOutput::Device { };

bool AudioOutput::available() { return false; }
bool AudioOutput::open() { return false; }
void AudioOutput::queue(const int16_t *, size_t) { }
void AudioOutput::stop() { m_pending.clear(); }
void AudioOutput::close() { }
#endif

AudioOutput::AudioOutput(uint32_t sampleRate)
    : m_sampleRate(sampleRate)
{
}

AudioOutput::~AudioOutput()
{
    close();
}

bool
AudioOutput::isOpen() const
{
    return m_device != nullptr;
}

void
AudioOutput::play(const int16_t *samples, size_t count)
{
    if (!m_device)
        return;
    const size_t buffer = std::max<size_t>(1, m_sampleRate / buffersPerSecond);
    m_pending.insert(m_pending.end(), samples, samples + count);
    size_t offset = 0;
    while (m_pending.size() - offset >= buffer) {
        queue(m_pending.data() + offset, buffer);
        offset += buffer;
    }
    m_pending.erase(m_pending.begin(), m_pending.begin() + static_cast<std::ptrdiff_t>(offset));
}

}
