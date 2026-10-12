/*
 * File: AudioSceneNode.cpp
 *
 * A SceneNode controlling an Audio effect in 3d space - implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#ifndef ORKA_DISABLE_AUDIO
#include "AudioSceneNode.hpp"
#include "../whaleml/WhaleMLFile.hpp"
#include "../essentials/Logger.hpp"
#include "../utilities/Tools.hpp"

#include <filesystem>
#include <functional>

static std::string buildSoundPath(std::string_view folder, std::string_view file) {
    std::filesystem::path filePath{std::string(file)};
    if (filePath.is_absolute()) return filePath.string();
    if (folder.empty() || folder == ".") return filePath.string();
    return (std::filesystem::path(std::string(folder)) / filePath).lexically_normal().string();
}

static void logAudioAssetLoad(std::string_view path) {
    std::error_code ec;
    const std::uintmax_t bytes = std::filesystem::file_size(path, ec);
    const std::string filename = std::filesystem::path(path).filename().string();
    std::string message = "Loading audio asset '" + filename + "'";
    LOG_ERROR_TYPE level = LET_INFO;

    if (!ec) {
        constexpr std::uintmax_t bytesPerMiB = 1024u * 1024u;
        message += " (" + MiscTools::formatByteSize(bytes) + ")";
        if (bytes >= 256u * bytesPerMiB) {
            static constexpr const char* comments[] = {
                "That is less a sound effect and more an audio documentary. Loading may take a while.",
                "This sound file has achieved feature-film status. Decoding will need a moment.",
                "Someone appears to have recorded the entire atmosphere. Please stand by."
            };
            level = LET_WARN;
            message += ". " + std::string(comments[std::hash<std::string>{}(filename) % 3]);
        } else if (bytes >= 64u * bytesPerMiB) {
            static constexpr const char* comments[] = {
                "This sound has main-character energy; decoding may take a moment.",
                "A surprisingly muscular sound file is entering the mixer.",
                "That is a lot of waveform for one file. The decoder is crying first."
            };
            level = LET_WARN;
            message += ". " + std::string(comments[std::hash<std::string>{}(filename) % 3]);
        }
    }

    Logger::log(level, "Orka, AudioSceneNode", message);
}

void AudioSceneNode::loadFile(std::string_view path) {
    WhaleMLFile db;
    try {
        db.load(std::string(path));
    } catch (std::exception& e) {
        Logger::log(LET_ERROR, "Orka, Sound File Parser", "File '" + std::string(path) + "' could not be loaded: " + std::string(e.what()));
        return;
    }

    std::string file = db.string("audio.file", "");
    if (file.empty()) {
        Logger::log(LET_WARN, "Orka, Sound File Parser", "File '" + std::string(path) + "' is no valid .sound.whaleml file! Missing audio.file.");
        return;
    }

    const std::string rootPath = MiscTools::getPath(path);
    const std::string soundPath = buildSoundPath(rootPath, file);
    Sound& sound = Sound::create();
    logAudioAssetLoad(soundPath);
    if (!sound.loadFile(soundPath, SLM_3D_SOUND)) {
        sound.destroy();
        return;
    }

    sound.setLoop(db.boolean("audio.loop", false));
    sound.setVolume(db.floating("audio.gain", 1.0f));
    const bool dopplerEnabled = db.boolean("doppler.enable", true);
    sound.setDopplerFactor(dopplerEnabled ? db.floating("doppler.factor", 1.0f) : 0.0f);
    const float loopPoint = db.floating("loop.point", -1.0f);
    if (loopPoint >= 0.0f) sound.setLoopPoint(loopPoint);

    setSound(sound);
    m_ownsSound = true;

    const std::string script = db.string("script.file", "");
    const bool instanceScript = db.boolean("script.instance", true);
    SceneNode::create(script, instanceScript);
}

void AudioSceneNode::loadAudioFile(std::string_view path) {
    Sound& sound = Sound::create();
    logAudioAssetLoad(path);
    if (!sound.loadFile(path, SLM_3D_SOUND)) {
        sound.destroy();
        return;
    }
    setSound(sound);
    m_ownsSound = true;
}

Sound& AudioSceneNode::getSound() {
    try {
        return AudioEngine::getSound(m_soundID);
    } catch (...) {
        Logger::log(LET_ERROR, "Orka, AudioSceneNode", "You destroyed a Sound object manually while it was bound to a SceneNode. Destroying the affected AudioSceneNode now...");
        throw std::runtime_error("AudioSceneNode references a destroyed Sound object. See log.");
    }
}

void AudioSceneNode::update(float delta) {
    updateScript(delta);
    Sound* sound = AudioEngine::findSound(m_soundID);
    if (sound == nullptr || !sound->isLoaded()) return;

    const vector3df position = getPosition();
    vector3df velocity(0.0f, 0.0f, 0.0f);
    if (m_hasLastPosition && delta > 0.0f) {
        velocity = (position - m_lastPosition) / delta;
    }

    sound->setPosition(position.x, position.y, position.z);
    sound->setVelocity(velocity.x, velocity.y, velocity.z);
    m_lastPosition = position;
    m_hasLastPosition = true;
}

void AudioSceneNode::destroy() {
    destroyOwnedSound();
    SceneNode::destroy();
}

void AudioSceneNode::destroyOwnedSound() {
    if (m_ownsSound) {
        Sound* sound = AudioEngine::findSound(m_soundID);
        if (sound != nullptr) sound->destroy();
    }
    m_soundID = -1;
    m_ownsSound = false;
    m_hasLastPosition = false;
}

AudioSceneNode::~AudioSceneNode() {
    destroyOwnedSound();
}

#endif

