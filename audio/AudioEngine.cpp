#ifndef ORKA_DISABLE_AUDIO
/*
 * File: AudioEngine.cpp
 *
 * Audio manager implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "AudioEngine.hpp"

#include "../3d/SceneManager.hpp"
#include "../primitives/audio/Sound.hpp"

#include <stdexcept>

#include "essentials/Logger.hpp"
#include "ext_stb_vorbis.h"

ma_engine AudioEngine::sm_engine;
std::unordered_map<Identifier, Sound*> AudioEngine::sm_sounds;
Identifier AudioEngine::sm_IDCount = 0;
bool AudioEngine::sm_autoListenerTracking = true;
bool AudioEngine::sm_hasLastAutoListenerPosition = false;
vector3df AudioEngine::sm_lastAutoListenerPosition = vector3df(0.0f, 0.0f, 0.0f);

void AudioEngine::init() {
    ma_engine_config conf = ma_engine_config_init();
    conf.sampleRate = 0;
    conf.channels = 0;
    const ma_result result = ma_engine_init(&conf, &sm_engine);
    if (result != MA_SUCCESS) {
        Logger::log(LET_WARN, "Orka, Audio Engine", "Audio Engine Init failed! NO AUDIO OUTPUT POSSIBLE!", false);
        exit(-1);
    }
}

void AudioEngine::deinit() {
    reset();
    ma_engine_uninit(&sm_engine);
}

void AudioEngine::reset() {
    std::unordered_map<Identifier, Sound*> soundsToDestroy;
    soundsToDestroy.swap(sm_sounds);
    sm_IDCount = 0;
    sm_hasLastAutoListenerPosition = false;
    for (auto& [id, sound] : soundsToDestroy) {
        delete sound;
    }
}

Sound* AudioEngine::findSound(Identifier ID) {
    const auto it = sm_sounds.find(ID);
    return it == sm_sounds.end() ? nullptr : it->second;
}

Sound& AudioEngine::getSound(Identifier ID) {
    Sound* sound = findSound(ID);
    if (!sound) {
        throw std::runtime_error("Sound not found: " + std::to_string(ID.getValue()));
    }
    return *sound;
}

bool AudioEngine::hasSound(Identifier ID) {
    return findSound(ID) != nullptr;
}

std::vector<Identifier> AudioEngine::getAllSounds() {
    std::vector<Identifier> IDs;
    IDs.reserve(sm_sounds.size());
    for (const auto& [id, sound] : sm_sounds) {
        IDs.push_back(id);
    }
    return IDs;
}

void AudioEngine::pauseAll() {
    for (auto& [id, sound] : sm_sounds) {
        if (sound && sound->isGlobalAudio()) {
            sound->pause();
        }
    }
}

void AudioEngine::fadeOutAll() {
    for (auto& [id, sound] : sm_sounds) {
        if (sound && sound->isGlobalAudio()) {
            sound->fadeOut(0.0f, 1000);
        }
    }
}

void AudioEngine::setAutoListenerTracking(bool enable) {
    sm_autoListenerTracking = enable;
    sm_hasLastAutoListenerPosition = false;
}

bool AudioEngine::isAutoListenerTrackingEnabled() {
    return sm_autoListenerTracking;
}

void AudioEngine::setListenerPosition(float x, float y, float z) {
    ma_engine_listener_set_position(&sm_engine, 0, x, y, z);
}

void AudioEngine::setListenerVelocity(float vx, float vy, float vz) {
    ma_engine_listener_set_velocity(&sm_engine, 0, vx, vy, vz);
}

void AudioEngine::update(float deltaTime) {
    if (sm_autoListenerTracking && SceneManager::hasCamera()) {
        const vector3df position = SceneManager::getCamera().getPosition();
        vector3df velocity(0.0f, 0.0f, 0.0f);
        if (sm_hasLastAutoListenerPosition && deltaTime > 0.0f) {
            velocity = (position - sm_lastAutoListenerPosition) / deltaTime;
        }
        setListenerPosition(position.x, position.y, position.z);
        setListenerVelocity(velocity.x, velocity.y, velocity.z);
        sm_lastAutoListenerPosition = position;
        sm_hasLastAutoListenerPosition = true;
    } else {
        sm_hasLastAutoListenerPosition = false;
    }

    for (auto& [id, sound] : sm_sounds) {
        if (sound) {
            sound->update(deltaTime);
        }
    }
}

void AudioEngine::addSound(Sound* sound) {
    if (!sound) {
        return;
    }

    const Identifier ID = createID();
    sound->setID(ID);
    sm_sounds[ID] = sound;
}

Identifier AudioEngine::createID() {
    return sm_IDCount++;
}

void AudioEngine::removeSound(Identifier ID) {
    const auto it = sm_sounds.find(ID);
    if (it == sm_sounds.end()) {
        return;
    }
    Sound* sound = it->second;
    sm_sounds.erase(it);
    delete sound;
}

Sound* AudioEngine::findLoadedFileInstance(std::string_view filepath, Identifier excludingID, bool spatialized) {
    for (auto& [id, sound] : sm_sounds) {
        if (id == excludingID || !sound || !sound->isLoaded()) {
            continue;
        }
        if (sound->m_filepath == filepath && sound->is3DSound() == spatialized) {
            return sound;
        }
    }
    return nullptr;
}

ma_engine& AudioEngine::getMiniAudioEngine() {
    return sm_engine;
}

#endif

