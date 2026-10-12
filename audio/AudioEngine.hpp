/*
 * File: AudioEngine.hpp
 *
 * Audio manager backed by MiniAudio.
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
#include <unordered_map>
#include <vector>

#include "ext_miniaudio.h"
#include "../primitives/core/MathTypes.hpp"
#include "../essentials/Identifier.hpp"

class Sound;

class AudioEngine {
public:
    // Initializes and tears down the global MiniAudio-backed sound manager.
    static void init();
    static void deinit();
    static void reset();

    // Looks up AudioEngine-managed Sound objects by their manager ID.
    static Sound* findSound(Identifier ID);
    static Sound& getSound(Identifier ID);
    static bool hasSound(Identifier ID);
    static std::vector<Identifier> getAllSounds();

    // Applies global playback control to streamed/global sounds.
    static void pauseAll();
    static void fadeOutAll();

    // Updates listener state and per-sound effects that need frame time.
    static void setAutoListenerTracking(bool enable=true);
    static bool isAutoListenerTrackingEnabled();
    static void setListenerPosition(float x, float y, float z);
    static void setListenerVelocity(float vx, float vy, float vz);
    static void update(float deltaTime);

private:
    friend class Sound;

    static void addSound(Sound* sound);
    static Identifier createID();
    static void removeSound(Identifier ID);
    static Sound* findLoadedFileInstance(std::string_view filepath, Identifier excludingID, bool spatialized);
    static ma_engine& getMiniAudioEngine();

    static ma_engine sm_engine;
    static std::unordered_map<Identifier, Sound*> sm_sounds;
    static Identifier sm_IDCount;
    static bool sm_autoListenerTracking;
    static bool sm_hasLastAutoListenerPosition;
    static vector3df sm_lastAutoListenerPosition;
};

