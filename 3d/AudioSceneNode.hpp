/*
 * File: AudioSceneNode.hpp
 *
 * A SceneNode controlling an Audio effect in 3d space
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#ifndef ORKA_DISABLE_AUDIO
#pragma once
#include "SceneNode.hpp"
#include "../audio/AudioEngine.hpp"
#include "../primitives/audio/Sound.hpp"

class AudioSceneNode : public CreatableSceneNode<AudioSceneNode> {
public:
    using CreatableSceneNode::create;

    // Loads .sound.whaleml. Will automatically set audio to sound effect/3d mode
    void loadFile(std::string_view path);
    // Loads audio file (MP3, FLAC, OGG, WAV...) will automatically set sound effect/3d mode
    void loadAudioFile(std::string_view path);

    // Manually sets Sound. Will leak currently Sound of not deleted before.
    void setSound(Sound& sound) { m_soundID = sound.getID(); m_ownsSound = false; m_hasLastPosition = false; }
    // Returns underlying Sound object
    Sound& getSound();

    // Destroys this AudioSceneNode and its owned Sound
    void destroy() override;

    inline SceneNodeType getType() const override { return SNT_AUDIO; };

private:
    friend class SceneNode;
    inline AudioSceneNode(bool registerInSceneManager = true) : CreatableSceneNode(registerInSceneManager) {}
    ~AudioSceneNode() override;
    // Updates the sound transform from this SceneNode
    void update(float delta) override;
    void destroyOwnedSound();
private:
    Identifier m_soundID;
    bool m_ownsSound = false;
    vector3df m_lastPosition = vector3df(0.0f, 0.0f, 0.0f);
    bool m_hasLastPosition = false;
};
#endif

