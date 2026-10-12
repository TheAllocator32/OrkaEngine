#ifndef ORKA_DISABLE_GAMEPLAY
/*
 * File: GameplayManager.cpp
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

#include "GameplayManager.hpp"

GameMode& GameplayManager::getGameMode() {
    if (!sm_currentGameMode) {
        throw std::runtime_error("You called 'getGameMode' but... There is no GameMode currently active!");
    }
    return *sm_currentGameMode;
}

void GameplayManager::unloadCurrentGameMode() {
    if (!sm_currentGameMode) {
        return;
    }

    sm_currentGameMode->unload();
    sm_currentGameMode.reset();
}

void GameplayManager::update(float deltaTime) {
    if (sm_currentGameMode) {
        sm_currentGameMode->update(deltaTime);
    }
}

#endif

