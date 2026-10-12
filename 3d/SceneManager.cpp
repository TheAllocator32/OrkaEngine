/*
 * File: SceneManager.cpp
 *
 * The SceneManager controlls the SceneNodes
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#ifndef ORKA_DISABLE_GUI
#include "gui/GUIManager.hpp"
#endif
#include "SceneManager.hpp"
#include "Scene.hpp"
#include "SceneNode.hpp"
#include "ModelSceneNode.hpp"
#include "essentials/Logger.hpp"
#include "../essentials/ProgressManager.hpp"
#ifndef ORKA_DISABLE_PHYSICS
#include "../physics/PhysicsEngine.hpp"
#endif
#include <algorithm>

void SceneManager::create() {
    ProgressManager::clear();
#ifndef ORKA_DISABLE_PHYSICS
    PhysicsEngine::init();
#endif
    sm_isActive = true;
}

void SceneManager::clear() {
#ifndef ORKA_DISABLE_PHYSICS
    PhysicsEngine::stopPhysicsThread();
#endif

    const auto sceneNodeIDs = getAllSceneNodes();
    for (const Identifier id : sceneNodeIDs) {
        if (sceneNodeExists(id)) getSceneNode(id).destroy();
    }
    if (sm_camera && !sm_camera->m_addedToSceneManager) sm_camera->destroy();

    // During shutdown, drain follow-up work too; ScriptEngine must be finished before nodes disappear.
    SceneNode::drainQueuedScriptOperations();
    flushDestroyedSceneNodes();
    sm_camera = nullptr;

    std::unordered_map<Identifier, Scene*> scenesToDestroy;
    auto scenes = sm_scenes.get();
    scenesToDestroy.swap(*scenes);
    scenes.unlock();
    for (auto& [id, scene] : scenesToDestroy) {
        (void)id;
        delete scene;
    }

    std::unordered_map<Identifier, SceneNode*> nodesToDestroy;
    auto sceneNodes = sm_sceneNodes.get();
    nodesToDestroy.swap(*sceneNodes);
    sm_IDCount = 0;
    sceneNodes.unlock();
    for (auto& [id, node] : nodesToDestroy) {
        delete node;
    }
    {
        auto queuedDestroys = sm_destroySceneNodeQueue.get();
        queuedDestroys->clear();
    }
#ifndef ORKA_DISABLE_PHYSICS
    PhysicsEngine::deinit();
#endif
    sm_isActive=false;
}

void SceneManager::addSceneNode(SceneNode* node) {
    auto sceneNodes = sm_sceneNodes.get();
    for (auto& [id, existing] : *sceneNodes) {
        if (existing == node) {
            Logger::log(LET_WARN, "Orka, Scene Manager", "SceneNode '" + std::to_string((long)node) + "' already added!");
            return;
        }
    }
    Identifier id = createID();
    node->setID(id);
    node->m_addedToSceneManager = true;
    (*sceneNodes)[id] = node;
}

void SceneManager::addScene(Scene* scene) {
    auto scenes = sm_scenes.get();
    for (const auto& [id, existing] : *scenes) {
        (void)id;
        if (existing == scene) {
            Logger::log(LET_WARN, "Orka, Scene Manager", "Scene '" + std::to_string((long)scene) + "' already added!");
            return;
        }
    }
    const Identifier id = createID();
    scene->setID(id);
    (*scenes)[id] = scene;
}

Identifier SceneManager::createID() {
    auto IDCountInternal = sm_IDCount.get();
    return (*IDCountInternal.get())++;
}

void SceneManager::setCamera(CameraSceneNode* camera) {
    if (sm_camera == camera) return;

    CameraSceneNode* oldCamera = sm_camera;
    sm_camera = camera;
    if (oldCamera) oldCamera->destroy();
}

void SceneManager::removeCamera(CameraSceneNode* camera) {
    if (sm_camera == camera) sm_camera = nullptr;
}

void SceneManager::update(float delta) {
#ifndef ORKA_DISABLE_PHYSICS
    PhysicsEngine::interpolateTransforms();
#endif
    const auto sceneNodeIDs = getAllSceneNodes();
    for (const auto& id : sceneNodeIDs) {
        if (!sceneNodeExists(id)) continue;
        auto& node = getSceneNode(id);
        if (node.isDestroyPending()) continue;
        node.update(delta);
    }
    if (sm_camera && !sm_camera->isDestroyPending()) sm_camera->update(delta);
    SceneNode::executeQueuedScriptOperations();
    updateModelCacheCleanup(delta);
}

bool SceneManager::sceneNodeExists(Identifier ID) {
    auto sceneNodes = sm_sceneNodes.get();
    auto it = sceneNodes->find(ID);
    if (it != sceneNodes->end() && it->second != nullptr)
        return !it->second->isDestroyPending();
    return false;
}

SceneNode& SceneManager::getSceneNode(Identifier ID) {
    auto sceneNodes = sm_sceneNodes.get();
    auto it = sceneNodes->find(ID);
    if (it == sceneNodes->end() || !it->second) throw std::runtime_error("SceneNode not found: " + std::to_string(ID.getValue()));
    return *it->second;
}

CameraSceneNode& SceneManager::getCamera() {
    if (sm_camera == nullptr) throw std::runtime_error("Camera not found");
    return *sm_camera;
}

std::vector<Identifier> SceneManager::getAllSceneNodes() {
    auto sceneNodes = sm_sceneNodes.get();
    std::vector<Identifier> values;
    values.reserve(sceneNodes->size());
    for (const auto& kv : *sceneNodes) {
        if (kv.second && kv.second->isDestroyPending()) continue;
        values.push_back(kv.first);
    }
    return values;
}

std::vector<Identifier> SceneManager::getSceneNodesWhere(const std::function<bool(const SceneNode&)>& predicate) {
    auto sceneNodes = sm_sceneNodes.get();
    std::vector<Identifier> values;
    values.reserve(sceneNodes->size());
    for (const auto& [id, node] : *sceneNodes) {
        if (node && !node->isDestroyPending() && predicate(*node)) values.push_back(id);
    }
    return values;
}

bool SceneManager::sceneExists(Identifier ID) {
    auto scenes = sm_scenes.get();
    auto it = scenes->find(ID);
    return it != scenes->end() && it->second != nullptr;
}

Scene& SceneManager::getScene(Identifier ID) {
    auto scenes = sm_scenes.get();
    auto it = scenes->find(ID);
    if (it == scenes->end() || !it->second) throw std::runtime_error("Scene not found: " + std::to_string(ID.getValue()));
    return *it->second;
}

std::vector<Identifier> SceneManager::getAllScenes() {
    auto scenes = sm_scenes.get();
    std::vector<Identifier> ids;
    ids.reserve(scenes->size());
    for (const auto& [id, scene] : *scenes) {
        if (scene) ids.push_back(id);
    }
    return ids;
}

void SceneManager::queueSceneNodeDestroy(Identifier ID) {
    auto destroyQueue = sm_destroySceneNodeQueue.get();
    if (std::find(destroyQueue->begin(), destroyQueue->end(), ID) != destroyQueue->end()) return;
    destroyQueue->push_back(ID);
}

void SceneManager::queueCameraDestroy(CameraSceneNode* camera) {
    if (!camera) return;

    auto destroyQueue = sm_destroyCameraQueue.get();
    if (std::find(destroyQueue->begin(), destroyQueue->end(), camera) != destroyQueue->end()) return;
    destroyQueue->push_back(camera);
}

void SceneManager::flushDestroyedSceneNodes() {
    std::vector<Identifier> idsToDestroy;
    auto destroyQueue = sm_destroySceneNodeQueue.get();
    idsToDestroy.swap(*destroyQueue);
    destroyQueue.unlock();

    std::vector<CameraSceneNode*> camerasToDestroy;
    auto cameraDestroyQueue = sm_destroyCameraQueue.get();
    camerasToDestroy.swap(*cameraDestroyQueue);
    cameraDestroyQueue.unlock();

#ifndef ORKA_DISABLE_PHYSICS
    PhysicsEngine::removeBodies(idsToDestroy);
#endif

    for (Identifier id : idsToDestroy) {
        SceneNode* node = nullptr;
        auto sceneNodes = sm_sceneNodes.get();
        auto it = sceneNodes->find(id);
        if (it != sceneNodes->end()) {
            node = it->second;
            sceneNodes->erase(it);
        }
        sceneNodes.unlock();
        if (node != nullptr) {
            node->setID(Identifier::INVALID);
            delete node;
        }
    }

    for (CameraSceneNode* camera : camerasToDestroy) {
        if (camera == nullptr) continue;
        if (sm_camera == camera) sm_camera = nullptr;
        delete camera;
    }
}

void SceneManager::updateModelCacheCleanup(float delta) {
    constexpr float kModelCacheIdleLifetimeSeconds = 15.0f;

    bool hasModelNodes = false;
    {
        auto sceneNodes = sm_sceneNodes.get();
        for (const auto& [id, node] : *sceneNodes) {
            (void)id;
            if (node && !node->isDestroyPending() && node->getType() == SNT_MODEL) {
                hasModelNodes = true;
                break;
            }
        }
    }

    bool hasPendingDestroys = false;
    {
        auto destroyQueue = sm_destroySceneNodeQueue.get();
        hasPendingDestroys = !destroyQueue->empty();
    }

    if (hasModelNodes || hasPendingDestroys) {
        sm_modelCacheIdleSeconds = 0.0f;
        sm_modelCachesClearedAfterIdle = false;
        return;
    }

    sm_modelCacheIdleSeconds += delta;
    if (!sm_modelCachesClearedAfterIdle && sm_modelCacheIdleSeconds >= kModelCacheIdleLifetimeSeconds) {
        ModelSceneNode::clearModelCaches();
        sm_modelCachesClearedAfterIdle = true;
    }
}

void SceneManager::removeSceneNode(Identifier ID) {
    SceneNode* node = nullptr;
    auto sceneNodes = sm_sceneNodes.get();
    auto it = sceneNodes->find(ID);
    if (it != sceneNodes->end()) {
        node = it->second;
    }
    sceneNodes.unlock();
    if (node) node->destroy();
}
