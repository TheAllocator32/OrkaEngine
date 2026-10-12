/*
 * File: GameplayManager.hpp
 *
 * Handles lifetime and update ownership of the currently active GameMode.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "GameMode.hpp"
#include <memory>
#include <stdexcept>

class GameplayManager {
public:
    template <GameModeSubclass GameModeType>
    inline static void setGameMode() {
        unloadCurrentGameMode();
        sm_currentGameMode = std::make_unique<GameModeType>();
        sm_currentGameMode->load();
    }

    template <StaticGameModeConcept GameModeType>
    inline static void setStaticGameMode() { setGameMode<StaticGameModeWrapper<GameModeType>>(); }

    static GameMode& getGameMode();

    static void unloadCurrentGameMode();
    static void update(float deltaTime);

private:
    inline static std::unique_ptr<GameMode> sm_currentGameMode;
};

