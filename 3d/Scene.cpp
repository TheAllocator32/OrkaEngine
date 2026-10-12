/*
 * File: WorldMap.cpp
 *
 * The Scene implementation (fileparser, apply, ...)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Scene.hpp"
#include "../essentials/Logger.hpp"
#include "../whaleml/WhaleMLFile.hpp"
#include "SceneManager.hpp"
#include "ModelSceneNode.hpp"
#include "AudioSceneNode.hpp"
#include "../3d/LightSceneNode.hpp"

#ifndef ORKA_DISABLE_PHYSICS
#include "../physics/PhysicsEngine.hpp"
#endif

Scene& Scene::create() {
    Scene* scene = new Scene();
    SceneManager::addScene(scene);
    return *scene;
}

static std::string getFileDirectory(std::string_view filepath) {
    const std::string path(filepath);
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? "" : path.substr(0, separator);
}

static std::string resolveSceneRelativePath(std::string_view sceneDirectory, std::string_view file) {
    if (file.empty() || sceneDirectory.empty()) return std::string(file);
    if (file.starts_with("/") || file.starts_with("\\") || (file.size() > 1 && file[1] == ':')) return std::string(file);
    return std::string(sceneDirectory) + "/" + std::string(file);
}

struct ScenePhysicsPause { // RAII thing
    ScenePhysicsPause() {
#ifndef ORKA_DISABLE_PHYSICS
        PhysicsEngine::pauseSimulation();
#endif
    }
    ~ScenePhysicsPause() {
#ifndef ORKA_DISABLE_PHYSICS
        PhysicsEngine::resumeSimulation();
#endif
    }
};

std::vector<Identifier> loadSceneNodes(WhaleMLFile& file, std::string_view sceneDirectory) {
    auto sceneObjectNames = file.getChildren("");
    std::vector<Identifier> nodes;
    std::string navPoint(file.getCurrentNavPoint());

    for (const auto& name : sceneObjectNames) {
        file.navigate(navPoint + "." + name);
        auto type = file.string("type", "model");
        if (type == "model") {
            bool success = true;
            auto path = file.string("file", "");
            auto position = file.vector3d("position", vector3df(0, 0, 0), success);
            auto rotation = file.quaternion("rotation", quaternionf(1, 0, 0, 0), success);
            auto scale = file.vector3d("scale", vector3df(1.0f, 1.0f, 1.0f), success);

            if (path.empty()) {
                Logger::log(LET_ERROR, "Orka, Scene", "Scene object '" + name + "' has no file entry!");
                continue;
            }
            if (!success) {
                Logger::log(LET_ERROR, "Orka, Scene", "Scene object '" + name + "' has malformed transform entries!");
                continue;
            }

            path = resolveSceneRelativePath(sceneDirectory, path);

            auto& node = ModelSceneNode::create();
            node.loadFile(path);
            node.setPosition(position);
            node.setRotation(rotation);
            node.setScale(scale);

            nodes.push_back(node.getID());
        }
        if (type == "light") {
            bool success = true;
            auto path = file.string("file", "");
            auto position = file.vector3d("position", vector3df(0, 0, 0), success);
            auto rotation = file.quaternion("rotation", quaternionf(1, 0, 0, 0), success);

            if (path.empty()) {
                Logger::log(LET_ERROR, "Orka, Scene", "Scene object '" + name + "' has no file entry!");
                continue;
            }
            if (!success) {
                Logger::log(LET_ERROR, "Orka, Scene", "Scene object '" + name + "' has malformed transform entries!");
                continue;
            }

            path = resolveSceneRelativePath(sceneDirectory, path);

            auto& node = LightSceneNode::create();
            node.loadFile(path);
            node.setPosition(position);
            node.setRotation(rotation);

            nodes.push_back(node.getID());
        }
    }

    file.navigate(navPoint);
    return nodes;
}

Scene::Scene() {
}

Scene::~Scene() {
    destroy();
}

void Scene::loadFile(std::string_view filepath) {
    ScenePhysicsPause physicsPause; // RAII magic to pause Physics
    destroy();
    const std::string sceneDirectory = getFileDirectory(filepath);

    WhaleMLFile file(filepath);
    if (!file.isValid()) {
        Logger::log(LET_ERROR, "Orka, Scene", "Could not load scene file '" + std::string(filepath) + "'!");
        return;
    }
    file.navigate("scene");
    auto sceneName = file.string("name", "Unnamed Scene! Usually means malformed .scene.whaleml or");
    m_name = sceneName;

    auto sceneNodeLODDistances = file.getValues("lod.distance");
    m_lodStages.stages.clear();
    m_lodStages.stages.reserve(sceneNodeLODDistances.size());
    for (const auto& distance : sceneNodeLODDistances)
        m_lodStages.stages.emplace_back(distance.asInt());
    m_lodStages.numStages = sceneNodeLODDistances.size();
    m_lodStages.stepPatternToContinue = m_lodStages.stages[m_lodStages.numStages - 1] - m_lodStages.stages[m_lodStages.numStages - 2];

    file.navigate("scene.objects");
    m_sceneNodes = loadSceneNodes(file, sceneDirectory);

    file.navigate("scene");
    auto chunks = file.getChildren("chunks");
    m_loaded = true;
    if (chunks.empty()) return;
    m_hasChunks = true;
    for (const auto& chunkName : chunks) {
        file.navigate("scene.chunks." + chunkName);
        std::unique_ptr<Chunk> chunk(new Chunk());
        auto chunkFile = file.getValue("file");
        if (!chunkFile.isValid()) {
            Logger::log(LET_ERROR, "Orka, Scene", "Chunk '" + chunkName + "' has no file entry!");
            continue;
        }
        chunk->loadFile(resolveSceneRelativePath(sceneDirectory, chunkFile.asString()));
        bool hasPosition = false;
        vector3df pos = file.vector3d("position", vector3df(0.0f, 0.0f, 0.0f), hasPosition);
        if (!hasPosition) {
            Logger::log(LET_ERROR, "Orka, Scene", "Chunk '" + chunkName + "' has no position entry!");
            continue;
        }
            chunk->setPosition(pos);
            m_chunks.push_back(std::move(chunk));
        }
}

void Scene::destroy() {
    for (const Identifier nodeID : m_sceneNodes) {
        if (!SceneManager::sceneNodeExists(nodeID)) continue;
        SceneManager::getSceneNode(nodeID).destroy();
    }
    m_sceneNodes.clear();

    for (const auto& chunk : m_chunks) {
        if (!chunk) continue;
        chunk->destroy();
    }
    m_chunks.clear();

    m_name.clear();
    m_hasChunks = false;
    m_loaded = false;
    m_lodStages = {};
    m_position = vector3df(0.0f, 0.0f, 0.0f);
}


void Scene::setPosition(vector3df pos) {
    const vector3df delta = pos - m_position;
    m_position = pos;

    for (const Identifier nodeID : m_sceneNodes) {
        if (!SceneManager::sceneNodeExists(nodeID)) continue;
        auto& node = SceneManager::getSceneNode(nodeID);
        node.setPosition(node.getPosition() + delta);
    }

    for (const auto& chunk : m_chunks) {
        if (!chunk) continue;
        chunk->setPosition(chunk->getPosition() + delta);
    }
}

std::vector<Identifier> Scene::getSceneNodes() const {
    std::vector<Identifier> result;

    result.append_range(m_sceneNodes);

    for (const auto& chunk : m_chunks) {
        result.append_range(chunk->getSceneNodes());
    }

    return result;
}

void Scene::Chunk::loadFile(std::string_view filepath) {
    ScenePhysicsPause physicsPause;
    const std::string chunkDirectory = getFileDirectory(filepath);

    WhaleMLFile file(filepath);
    if (!file.isValid()) {
        Logger::log(LET_ERROR, "Orka, Scene Chunk", "Could not load chunk file '" + std::string(filepath) + "'!");
        return;
    }

    file.navigate("chunk");
    m_name = file.string("name", "Unnamed Chunk");
    file.navigate("chunk.objects");
    m_sceneNodes = loadSceneNodes(file, chunkDirectory);
}

void Scene::Chunk::setPosition(vector3df pos) {
    const vector3df delta = pos - m_position;
    m_position = pos;

    for (const Identifier nodeID : m_sceneNodes) {
        if (!SceneManager::sceneNodeExists(nodeID)) continue;
        auto& node = SceneManager::getSceneNode(nodeID);
        node.setPosition(node.getPosition() + delta);
    }
}

void Scene::Chunk::destroy() {
    for (const Identifier nodeID : m_sceneNodes) {
        if (!SceneManager::sceneNodeExists(nodeID)) continue;
        SceneManager::getSceneNode(nodeID).destroy();
    }
    m_sceneNodes.clear();
    m_name.clear();
    m_position = vector3df(0.0f, 0.0f, 0.0f);
}
