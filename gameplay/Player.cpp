#ifndef ORKA_DISABLE_GAMEPLAY
/*
 * File: Player.cpp
 *
 * The Player class that can be overwritten with your own Player class! - Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Player.hpp"
#include "../whaleml/WhaleMLFile.hpp"
#include "../utilities/Tools.hpp"
#include <vector>

static std::string getFirstValidString(WhaleMLFile& db, const std::vector<std::string>& keys) {
    for (std::string_view key : keys) {
        std::string value = db.string(key, "");
        if (value.empty()) {
            continue;
        }
        return value;
    }
    return "";
}

void OrkaPlayerBase::load(std::string_view path) {
    WhaleMLFile db;
    db.load(path);

    std::string abilities = getFirstValidString(db, {"abilities.file", "Player.abilities.file", "player.abilities.file", "abilities"});
    std::string upgrades = getFirstValidString(db, {"upgrades.file", "Player.upgrades.file", "player.upgrades.file", "upgrades"});
    std::string music = getFirstValidString(db, {"music.file", "Player.music.file", "player.music.file", "music"});
    std::string scenenode = getFirstValidString(db, {"scenenode.file", "Player.scenenode.file", "player.scenenode.file", "scenenode"});
    std::string script = getFirstValidString(db, {"script.file", "Player.script.file", "player.script.file", "script"});

    if (abilities.empty() || upgrades.empty() || scenenode.empty() || script.empty()) {
        Logger::log(LET_ERROR, "Orka, Player (Character)", "Failed to load Player WhaleML file or required fields are missing: " + std::string(path));
        return;
    }

    m_rootPath = MiscTools::getPath(path);

    load(abilities, scenenode, script, music, upgrades);
}

void OrkaPlayerBase::load(std::string_view abilitiesFilename, std::string_view scenenodeFilename, std::string_view scriptFilename, std::string_view musicFilename, std::string_view upgradesFilename) {
    m_characterController = new CharacterController();
    m_upgradeHandler = new UpgradeHandler();
    m_sceneNode = &ModelSceneNode::create();
    m_characterController->setSceneNode(*m_sceneNode);
    m_upgradeHandler->initWithCharacterController(m_characterController);

    m_sceneNode->loadFile(m_rootPath + "/" + std::string(scenenodeFilename));
    m_characterController->loadActions(m_rootPath + "/" + std::string(abilitiesFilename));
    m_characterController->loadScript(m_rootPath + "/" + std::string(scriptFilename));
    m_upgradeHandler->load(m_rootPath + "/" + std::string(upgradesFilename));
}

void OrkaPlayerBase::act(std::string_view action) {
    m_characterController->invokeAction(action);
}

OrkaPlayerBase::~OrkaPlayerBase(){
    delete m_upgradeHandler;
    delete m_characterController;
}

#endif

