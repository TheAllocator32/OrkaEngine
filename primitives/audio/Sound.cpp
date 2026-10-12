/*
 * File: Sound.cpp
 *
 * AudioEngine-managed sound implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Sound.hpp"

#include "../../audio/AudioEngine.hpp"
#include "../../essentials/Logger.hpp"
#include <filesystem>
#include <system_error>

static std::string maErrorDescription(ma_result result) {
    switch (result) {
    case MA_SUCCESS:
        return "Operation successful (Is this an error??? NO! Congratulations!)";
    case MA_INVALID_ARGS:
        return "Invalid arguments provided (Coding error that should actually not happen!)";
    case MA_INVALID_OPERATION:
        return "Invalid operation (I'm sorry this is a coding error!)";
    case MA_OUT_OF_MEMORY:
        return "Out of memory (Phew! are you running this on a Raspberry Pi or is it just your 2GB ram potato PC?)";
    case MA_FORMAT_NOT_SUPPORTED:
        return "Audio format not supported (Why isn't FLAC OGG WAV or MP3 is enough for you?!? We DON'T support Apple fileformats!)";
    case MA_INVALID_FILE:
        return "Invalid file or unsupported format (Well... is your memory damaged?)";
    case MA_NO_BACKEND:
        return "No audio backend available (Maybe you forgot to plug your interface (or for our Windows users: forgot the ASIO drivers?))";
    case MA_ERROR:
        return "Unspecified error (Usually happens on Windows every 2 seconds)";
    default:
        return "Unknown error code... WTF!?!?";
    }
}

static std::string normalizeSoundPath(std::string_view filepath) {
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(filepath, error);
    if (!error) {
        return canonical.string();
    }
    return std::filesystem::path(filepath).lexically_normal().string();
}

Sound& Sound::create() {
    return *new Sound();
}

Sound::Sound() {
    AudioEngine::addSound(this);
}

Sound::~Sound() {
    unload();
}

bool Sound::loadFile(std::string_view filename, SoundLoadMode mode) {
    unload();
    m_filepath = normalizeSoundPath(filename);
    const ma_uint32 flags = mode == SLM_GLOBAL_AUDIO
        ? MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_ASYNC
        : MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;
    // MiniAudio can copy file-backed decoded sounds, but not streamed sounds.
    Sound* fileInstance = mode == SLM_3D_SOUND
        ? AudioEngine::findLoadedFileInstance(m_filepath, m_id, true)
        : nullptr;
    const ma_result result = fileInstance
        ? ma_sound_init_copy(&AudioEngine::getMiniAudioEngine(), &fileInstance->m_sound, flags, nullptr, &m_sound)
        : ma_sound_init_from_file(
            &AudioEngine::getMiniAudioEngine(),
            m_filepath.c_str(),
            flags,
            nullptr,
            nullptr,
            &m_sound
        );
    if (result != MA_SUCCESS) {
        Logger::log(LET_WARN, "Orka, Sound", "Sound " + std::to_string(m_id.getValue()) + " with path '" + m_filepath + "' could not be loaded: " + maErrorDescription(result), mode == SLM_GLOBAL_AUDIO);
        m_filepath.clear();
        return false;
    }

    m_loadMode = mode;
    m_loaded = true;
    return true;
}

void Sound::unload() {
    if (!m_loaded) {
        return;
    }
    ma_sound_uninit(&m_sound);
    m_sound = {};
    m_pitchFade = {};
    m_filepath.clear();
    m_loaded = false;
}

void Sound::destroy() {
    AudioEngine::removeSound(m_id);
}

void Sound::play() const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_start(const_cast<ma_sound*>(&m_sound));
}

void Sound::pause() const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_stop(const_cast<ma_sound*>(&m_sound));
}

void Sound::stop() const {
    pause();
}

void Sound::setLoop(bool loop) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_looping(const_cast<ma_sound*>(&m_sound), loop ? MA_TRUE : MA_FALSE);
}

void Sound::setLoopPoint(float seconds) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    if (seconds < 0.0f) {
Logger::log(LET_WARN, "Orka, Sound", "Loop point for Sound " + std::to_string(m_id.getValue()) + " cannot be negative.");
        return;
    }

    ma_uint64 lengthInFrames = 0;
    const ma_result lengthResult = ma_sound_get_length_in_pcm_frames(const_cast<ma_sound*>(&m_sound), &lengthInFrames);
    if (lengthResult != MA_SUCCESS || lengthInFrames == 0) {
Logger::log(LET_WARN, "Orka, Sound", "Could not set loop point for Sound " + std::to_string(m_id.getValue()) + ": sound length is unknown.");
        return;
    }

    const ma_uint64 loopStart = static_cast<ma_uint64>(seconds * ma_engine_get_sample_rate(&AudioEngine::getMiniAudioEngine()));
    if (loopStart >= lengthInFrames) {
Logger::log(LET_WARN, "Orka, Sound", "Loop point for Sound " + std::to_string(m_id.getValue()) + " is outside the sound length.");
        return;
    }

    const ma_result loopResult = ma_data_source_set_loop_point_in_pcm_frames(m_sound.pDataSource, loopStart, lengthInFrames);
    if (loopResult != MA_SUCCESS) {
Logger::log(LET_WARN, "Orka, Sound", "Could not set loop point for Sound " + std::to_string(m_id.getValue()) + ": " + maErrorDescription(loopResult));
    }
}

void Sound::setVolume(float volume) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_volume(const_cast<ma_sound*>(&m_sound), volume);
}

void Sound::fadeIn(float volume, int milliseconds) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_fade_in_milliseconds(const_cast<ma_sound*>(&m_sound), 0, volume, milliseconds);
}

void Sound::fadeOut(float volume, int milliseconds) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_fade_in_milliseconds(const_cast<ma_sound*>(&m_sound), -1, volume, milliseconds);
}

void Sound::setPitch(float pitch) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_pitch(const_cast<ma_sound*>(&m_sound), pitch);
}

void Sound::fadePitch(float targetPitch, int milliseconds) {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    if (milliseconds <= 0) {
        setPitch(targetPitch);
        m_pitchFade = {};
        return;
    }

    m_pitchFade.currentPitch = ma_sound_get_pitch(&m_sound);
    m_pitchFade.targetPitch = targetPitch;
    m_pitchFade.duration = milliseconds / 1000.0f;
    m_pitchFade.elapsedTime = 0.0f;
    m_pitchFade.pitchIncrement = (targetPitch - m_pitchFade.currentPitch) / m_pitchFade.duration;
    m_pitchFade.active = true;
}

void Sound::setPosition(float x, float y, float z) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_position(const_cast<ma_sound*>(&m_sound), x, y, z);
}

void Sound::setVelocity(float vx, float vy, float vz) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_velocity(const_cast<ma_sound*>(&m_sound), vx, vy, vz);
}

void Sound::setDopplerFactor(float factor) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_doppler_factor(const_cast<ma_sound*>(&m_sound), factor);
}

void Sound::setMinDistance(float minDistance) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_min_distance(const_cast<ma_sound*>(&m_sound), minDistance);
}

void Sound::setMaxDistance(float maxDistance) const {
    if (!m_loaded) {
        logMissingSound();
        return;
    }
    ma_sound_set_max_distance(const_cast<ma_sound*>(&m_sound), maxDistance);
}

void Sound::update(float deltaTime) {
    if (!m_pitchFade.active || !m_loaded) {
        return;
    }

    m_pitchFade.elapsedTime += deltaTime;
    if (m_pitchFade.elapsedTime < m_pitchFade.duration) {
        const float newPitch = m_pitchFade.currentPitch + m_pitchFade.pitchIncrement * deltaTime;
        setPitch(newPitch);
        m_pitchFade.currentPitch = newPitch;
        return;
    }

    setPitch(m_pitchFade.targetPitch);
    m_pitchFade = {};
}

void Sound::logMissingSound() const {
Logger::log(LET_WARN, "Orka, Sound", "Sound " + std::to_string(m_id.getValue()) + " has not been loaded yet.");
}

