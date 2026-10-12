/*
 * File: UpgradeHandler.hpp
 *
 * This is a simple upgrade system reference implementation! You can integrate it for example together with the CharacterController into your Player
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "CharacterController.hpp"
#include <memory>
#include <vector>
#include <string>

struct Upgrade {
    std::string id;
    std::string func;
    std::string name;
    std::string description;
    int maxLevel;
    std::vector<int> steps;
};

class UpgradeHandler {
public:
    // Bonds the UpgradeSystem directly to the CharacterController and automatically activates upgrade functions etc. (call apply after that)
    inline void initWithCharacterController(CharacterController* characterController) { m_controller = characterController; }
    // Loads a .upgrades.whaleml
    void load(std::string_view path);
    // Applies all passed upgrades to the CharacterController 
    void apply(std::string_view id, int level);
    void apply(std::vector<std::string> ids, std::vector<int> levels);
    // Returns the descriptions and their ID (1. ID 2. description)
    inline std::vector<Upgrade> getUpgrades() { return m_upgrades; }
private:
    CharacterController* m_controller = nullptr;
    std::vector<Upgrade> m_upgrades;
};

