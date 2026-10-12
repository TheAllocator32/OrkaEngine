/*
 * File: Sound.hpp
 *
 * AudioEngine-managed sound object.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

#include <string>
#include <string_view>

#include "../../essentials/Identifier.hpp"
#include "../../audio/ext_miniaudio.h"

enum SoundLoadMode {
    SLM_GLOBAL_AUDIO,    // Use for music, is played without 3d effects/positioning
    SLM_3D_SOUND        // Use for sound effects
};

class AudioEngine;
class AudioSceneNode;

class Sound {
public:
    static Sound& create();

    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;
    Sound(Sound&&) = delete;
    Sound& operator=(Sound&&) = delete;

    // Loads an audio file (MP3, OGG, FLAC, WAV...). Use AudioSceneNode for .sound.whaleml and MusicManager for .music.whaleml. Global audio is suited for music and streams from disk. 3D sounds are suited for SFX and decode into RAM for low-latency playback.
    bool load(std::string_view filename, SoundLoadMode mode = SLM_GLOBAL_AUDIO) { return loadFile(filename, mode); }
    bool loadFile(std::string_view filename, SoundLoadMode mode = SLM_GLOBAL_AUDIO);
    // Removes the currently loaded audio from this managed Sound without deleting the Sound object.
    void unload();
    // Removes this Sound from AudioEngine and deletes it.
    void destroy();

    // Plays the audio file loaded into this Sound.
    void play() const;
    // It's like a CD player: play, pause, stop.
    void pause() const;
    void stop() const;
    // Loops audio playback. Mostly meant for music; short repeating SFX get noisy quickly.
    void setLoop(bool loop = true) const;
    // Sets the point where looping restarts, in seconds from the beginning of the file.
    void setLoopPoint(float seconds) const;

    // Sets or fades the volume.
    void setVolume(float volume) const;
    void fadeIn(float volume, int milliseconds) const;
    void fadeOut(float volume, int milliseconds) const;
    // Pitch shifting can be used to simulate an old disk or other playback effects.
    void setPitch(float pitch) const;
    void fadePitch(float targetPitch, int milliseconds);

    // Sets doppler and distance behavior for spatial sounds.
    void setDopplerFactor(float factor) const;
    void setMinDistance(float minDistance) const;
    void setMaxDistance(float maxDistance) const;

    bool isLoaded() const { return m_loaded; }
    bool isGlobalAudio() const { return m_loaded && m_loadMode == SLM_GLOBAL_AUDIO; }
    bool is3DSound() const { return m_loaded && m_loadMode == SLM_3D_SOUND; }
    Identifier getID() const { return m_id; }
    std::string_view getFilePath() const { return m_filepath; }

private:
    friend class AudioEngine;
    friend class AudioSceneNode;

    struct PitchFade {
        float currentPitch = 1.0f;
        float targetPitch = 1.0f;
        float pitchIncrement = 0.0f;
        float duration = 0.0f;
        float elapsedTime = 0.0f;
        bool active = false;
    };

    Sound();
    ~Sound();

    void update(float deltaTime);
    void logMissingSound() const;
    void setID(Identifier id) { m_id = id; }
    void setPosition(float x, float y, float z) const;
    void setVelocity(float vx, float vy, float vz) const;

    Identifier m_id;
    std::string m_filepath;
    ma_sound m_sound{};
    SoundLoadMode m_loadMode = SLM_GLOBAL_AUDIO;
    PitchFade m_pitchFade;
    bool m_loaded = false;
};

