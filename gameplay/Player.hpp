/*
 * File: Player.hpp
 *
 * The Player class that can be overwritten with your own Player class!
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

/*
 * Player.whaleml fields:
 * abilities.file : "file.action.whaleml"
 * scenenode.file : "file.model.whaleml"
 * script.file : "file.as"
 * music.file : "file.music.whaleml"
 * upgrades.file : "file.upgrades.whaleml"
 */

#pragma once
#include "CharacterController.hpp"
#include "UpgradeHandler.hpp"
#include <stdexcept>

class OrkaPlayerBase {
public:
    // Loads a Player (Character) from its WhaleML file. (Only load 1 player per 2 frames to avoid hang-ons)
    virtual void load(std::string_view path);
    // Loads the Player manually. Looks lightweight but is as slow as Windows 11 boot on a HDD since we load 3D models, textures and scripts here so only load one player per 2 frames to avoid the App "hang on" problems.
    virtual void load(std::string_view abilitiesFilename, std::string_view scenenodeFilename, std::string_view scriptFilename, std::string_view musicFilename, std::string_view upgradesFilename);
    // Returns the filepath of the Player.whaleml file
    inline std::string getFilePath() { return m_rootPath; }
    // Returns the underlying SceneNode
    inline ModelSceneNode& getSceneNode() { if (m_sceneNode == nullptr) throw std::runtime_error("Player SceneNode not loaded"); return *m_sceneNode; }
    // Returns the internal CharacterController
    inline CharacterController* getCharacterController() { return m_characterController; }
    // Updates the Character. This is usually done by GameMode::update so don't call manually.
    virtual void update() {}; // Stub

    virtual ~OrkaPlayerBase();

    // Plays an action/animation from the predefined actions (defined in the Player WhaleML files). Internally calls CharacterController::invokeAction
    virtual void act(std::string_view action);

protected:
    CharacterController* m_characterController = nullptr;
    UpgradeHandler* m_upgradeHandler = nullptr;
    ModelSceneNode* m_sceneNode = nullptr;
    std::string m_rootPath;
    std::string m_currentAnimation;
    std::string m_id; // Every player in the match has an unique identifier
};

