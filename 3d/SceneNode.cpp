/*
 * File: SceneNode.cpp
 *
 * Defines the SceneNode base structure - Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "SceneNode.hpp"
#include "ModelSceneNode.hpp"
#include "../essentials/Logger.hpp"
#include "../essentials/ProgressManager.hpp"
#ifndef ORKA_DISABLE_SCRIPTING
#include "../scripting/ScriptEngine.hpp"
#endif
#ifndef ORKA_DISABLE_PHYSICS
#include "../physics/PhysicsEngine.hpp"
#endif
#include "SceneManager.hpp"

#ifndef ORKA_DISABLE_SCRIPTING
template<typename... Args>
static void invokeTypedSceneNodeScript(SceneNode* node, Identifier scriptId, std::string_view functionName, Args&&... args) {
    switch (node->getType()) {
    case SNT_MODEL:
        try {
            ScriptEngine::invoke<void>(scriptId, functionName, static_cast<ModelSceneNode*>(node), std::forward<Args>(args)...);
            return;
        }
        catch (const std::exception&) {
            break;
        }
    default:
        break;
    }

    ScriptEngine::invoke<void>(scriptId, functionName, node, std::forward<Args>(args)...);
}
#endif

SceneNode& SceneNode::create() {
    return SceneNode::create<SceneNode>(true);
}

SceneNode::SceneNode(bool addToSceneManager) {
    if (addToSceneManager) SceneManager::addSceneNode(this);
}

void SceneNode::queueScriptOperation(std::function<void()> operation) {
    std::lock_guard lock(sm_scriptOperationsMutex);
    sm_scriptOperations.push_back(std::move(operation));
}

bool SceneNode::executeQueuedScriptOperations() {
    std::vector<std::function<void()>> operations;
    {
        std::lock_guard lock(sm_scriptOperationsMutex);
        operations.swap(sm_scriptOperations);
    }
    for (auto& operation : operations) operation();
    return !operations.empty();
}

void SceneNode::drainQueuedScriptOperations() {
    while (executeQueuedScriptOperations()) {}
}

void SceneNode::create(std::string_view scriptPath, bool instanceScript) {
    m_scriptPath = scriptPath;
    m_instanceScript = instanceScript;
#ifdef ORKA_DISABLE_SCRIPTING
    if (!scriptPath.empty()) {
        Logger::log(LET_WARN, "Orka, Scene Node, Scripting", "Script path ignored because ORKA_DISABLE_SCRIPTING is enabled.");
    }
    if (m_progressID != Identifier::INVALID) ProgressManager::step(m_progressID);
#else
    queueScriptOperation([this] {
        if (m_destroyPending) return;

        if (!m_scriptPath.empty()) {
            m_scriptID = m_instanceScript
                ? ScriptEngine::loadSharedScript(m_scriptPath, "scene")
                : ScriptEngine::loadScript(m_scriptPath, "scene");
            if (m_scriptID == Identifier::INVALID) {
                if (m_progressID != Identifier::INVALID) ProgressManager::step(m_progressID); // Script step finished, even if script loading failed.
                return;
            }
        }

        if (!m_scriptPath.empty()) { // If we don't have a path, we don't have a script lol
            try {
                invokeTypedSceneNodeScript(this, m_scriptID, "create");
            } catch (const std::exception& e) {
                // Script has no "create" defined
                Logger::log(LET_WARN, "Orka, Scene Node, Scripting", "No create function defined. Are you sure? " + std::string(e.what()));
            }
        }

        if (m_progressID != Identifier::INVALID) ProgressManager::step(m_progressID); // Script compiled / setup finished.
    });
#endif
}

void SceneNode::update(float delta) {
    updateScript(delta);
}

void SceneNode::updateScript(float delta) {
#ifndef ORKA_DISABLE_SCRIPTING
    if (m_scriptID != Identifier::INVALID) {
        queueScriptOperation([this, delta] {
            if (m_destroyPending || m_scriptID == Identifier::INVALID) return;

            try {
                invokeTypedSceneNodeScript(this, m_scriptID, "update", delta);
            }
            catch (std::exception e) {
                const std::string msg = e.what() ? e.what() : "";
                if (msg.rfind("Function not found:", 0) == 0) {
                    Logger::log(LET_WARN, "Orka, Scene Node, Scripting",
                        "No update() defined in script.");
                } else {
                    Logger::log(LET_ERROR, "Orka, Scene Node, Scripting",
                        std::string("Script error in update(): ") + msg);
                }
            }
        });
    }
#endif
}

void SceneNode::destroy() {
    if (m_destroyPending) return;
    m_destroyPending = true;
    m_visible = false;
    m_transformChangedCallback = nullptr;

#ifndef ORKA_DISABLE_SCRIPTING
    queueScriptOperation([this] {
        if (m_scriptID != Identifier::INVALID) {
            try {
                invokeTypedSceneNodeScript(this, m_scriptID, "destroy");
            } catch (std::exception e) {
                // Script has no unload script
                Logger::log(LET_WARN, "Orka, Scene Node, Scripting", "No destroy function defined. Are you sure? " + std::string(e.what()));
            }
        }
        if (m_scriptID != Identifier::INVALID) {
            ScriptEngine::unloadScript(m_scriptID);
            m_scriptID = Identifier::INVALID;
        }
#endif
        if (m_id != -1) {
            SceneManager::queueSceneNodeDestroy(m_id);
        }
#ifndef ORKA_DISABLE_SCRIPTING
    });
#else
    if (m_id != -1) SceneManager::queueSceneNodeDestroy(m_id);
#endif
}

void SceneNode::setCustomUserWhaleMLFile(const WhaleMLFile& tag) {
    if (!m_customUserWhaleMLFile) {
        m_customUserWhaleMLFile = std::make_unique<WhaleMLFile>(tag);
    } else {
        *m_customUserWhaleMLFile = tag;
    }
}

