/*
 * File: GameMode.hpp
 * 
 * Contains abstract GameMode class.
 * 
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <concepts>

// Abstract :/
class GameMode {
public:
    // The Constructor! Should only do instantiation and construction of the subsystems
    inline GameMode() {};
    // Destructor! Should unload all remaining systems. called after deinitializeAll
    virtual ~GameMode() = default;

    // Implement the actual loading here
    virtual void load()=0;

    // Implement unloading here
    virtual void unload()=0;

    // Custom update function called every frame
    virtual void update(float delta)=0;
};


// A wrapper around static GameModes to make them feel dynamic.
template<typename T>
concept StaticGameModeConcept = requires(float dt) {
    T::update(dt);
};

template<StaticGameModeConcept StaticGameMode>
class StaticGameModeWrapper : public GameMode {
public:
    StaticGameModeWrapper(bool loadOnStart = false, bool unloadAtExit = false) : m_load(loadOnStart), m_unload(unloadAtExit) {}

    void load() override {
        if constexpr (requires { StaticGameMode::load(); }) {
            if (m_load)
                StaticGameMode::load();
        }
    }

    void update(float dt) override {
        StaticGameMode::update(dt);
    }

    void unload() override {
        if constexpr (requires { StaticGameMode::unload(); }) {
            if (m_unload)
                StaticGameMode::unload();
        }
    }

private:
    bool m_load = false;
    bool m_unload = false;
};

template <typename T>
concept GameModeSubclass = std::derived_from<T, GameMode>;

