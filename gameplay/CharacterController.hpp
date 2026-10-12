/*
 * File: CharacterController.hpp
 *
 * The Character Controller loads an AngelScript file, can invoke actions defined inside it and also exposes itself to the script so that it can call functions like "move to position XY"!
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../3d/SceneNode.hpp"
#include "../3d/ModelSceneNode.hpp"
#include "../3d/SceneManager.hpp"
#include "../scripting/ScriptEngine.hpp"
#include "../essentials/Logger.hpp"

struct Action {
    std::string id;
    std::string func;
    std::string args;
};

// This class is fully available from Script!
class CharacterControllerScripted {
public:

};

// The CharacterController itself
class CharacterController {
public:
    // Hmmm..
    inline void setSceneNode(SceneNode& sceneNode) { m_sceneNode = sceneNode.getID(); }
    inline void setSceneNode(Identifier sceneNode) { m_sceneNode = sceneNode; }

    // Sets/loads the Character's AngelScript File!
    void loadScript(std::string_view path);

    // Loads .actions.whaleml file
    void loadActions(std::string_view path);

    // Invokes/runs an action defined in the Character's config and AngelScript File!
    template<typename... ScriptArgs>
    inline void invokeAction(std::string_view actionName, ScriptArgs&&... arguments) {
        auto it = std::find_if(m_actions.begin(), m_actions.end(), [actionName](const Action& e) {return e.id == actionName; });
        if (it == m_actions.end()) { Logger::log(LET_WARN, "Orka, Character Controller, invoke", "Action '" + std::string(actionName) + "' not found!"); return;}
        ScriptEngine::invoke<void>(m_scriptID, it->func, m_cnt, std::forward<ScriptArgs>(arguments)...);
    }

    // Invokes a function (You're probably looking for "invokeAction". This is only the low level function calling) The function parameter should be the name of the function without brackets
    template<typename... ScriptArgs>
    inline void invokeFunction(std::string_view function, ScriptArgs&&... arguments) { ScriptEngine::invoke<void>(m_scriptID, function, std::forward<ScriptArgs>(arguments)...); }
private:
    Identifier m_scriptID = Identifier::INVALID;
    std::vector<Action> m_actions;
    Identifier m_sceneNode;
    CharacterControllerScripted* m_cnt = nullptr;
};

