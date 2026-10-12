#ifndef ORKA_DISABLE_GAMEPLAY
/*
 * File: CharacterController.cpp
 *
 * Implements the CharacterController
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "CharacterController.hpp"
#include "../whaleml/WhaleMLFile.hpp"

static std::string normalizeValue(const WhaleMLScalarValue& value) {
    std::string out = value.asString();
    if (out.size() >= 2) {
        const bool doubleQuoted = out.front() == '"' && out.back() == '"';
        const bool singleQuoted = out.front() == '\'' && out.back() == '\'';
        if (doubleQuoted || singleQuoted) {
            out = out.substr(1, out.size() - 2);
        }
    }
    return out;
}

void CharacterController::loadScript(std::string_view path) {
    m_scriptID = ScriptEngine::loadScript(path);
}

void CharacterController::loadActions(std::string_view path) {
    WhaleMLFile db(path);

    auto loadKeyedActions = [&](std::string_view root) {
        const std::string rootString(root);
        std::vector<std::string> actionIds = db.getChildren(rootString);
        int loaded = 0;
        for (std::string_view actionId : actionIds) {
            if (actionId.empty()) {
                continue;
            }
            const std::string actionIdString(actionId);

            WhaleMLScalarValue funcValue = db[rootString + "." + actionIdString + ".func"];
            if (!funcValue.isValid()) {
                continue;
            }

            Action a;
            a.id = actionIdString;
            a.func = normalizeValue(funcValue);
            WhaleMLScalarValue extraArgsValue = db[rootString + "." + actionIdString + ".extraargs"];
            if (extraArgsValue.isValid()) {
                a.args = normalizeValue(extraArgsValue);
            }
            m_actions.push_back(a);
            ++loaded;
        }
        return loaded;
    };

    int loadedActions = 0;
    for (std::string_view root : {"actions", "Actions", "action"}) {
        loadedActions += loadKeyedActions(root);
        if (loadedActions > 0) break;
    }

    auto loadIndexedActions = [&](std::string_view root, int startIndex) {
        int loaded = 0;
        for (int i = startIndex;; ++i) {
            const std::string indexedRoot = std::string(root) + "." + std::to_string(i);
            WhaleMLScalarValue id = db[indexedRoot + ".id"];
            WhaleMLScalarValue func = db[indexedRoot + ".func"];

            if (!id.isValid() && !func.isValid()) {
                break;
            }

            Action a;
            if (id.isValid()) a.id = normalizeValue(id);
            if (func.isValid()) a.func = normalizeValue(func);
            WhaleMLScalarValue args = db[indexedRoot + ".extraargs"];
            if (args.isValid()) a.args = normalizeValue(args);
            m_actions.push_back(a);
            ++loaded;
        }
        return loaded;
    };

    if (loadedActions == 0) {
        for (std::string_view root : {"actions", "Actions", "actions.action", "Actions.action", "action"}) {
            loadedActions += loadIndexedActions(root, 0);
            if (loadedActions > 0) break;
        }
    }
    if (loadedActions == 0) {
        for (std::string_view root : {"actions", "Actions", "actions.action", "Actions.action", "action"}) {
            loadedActions += loadIndexedActions(root, 1);
            if (loadedActions > 0) break;
        }
    }

    if (loadedActions == 0) {
        Logger::log(LET_WARN, "Orka, Character Controller", "No actions loaded from WhaleML file: " + std::string(path));
    }
}

#endif

