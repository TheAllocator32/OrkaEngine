/*
 * File: ModelSceneNode.cpp
 *
 * A SceneNode containing a mesh. Can be described using .model.whaleml - Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "ModelSceneNode.hpp"
#include "../essentials/ProgressManager.hpp"
#include "../essentials/Logger.hpp"
#include "../utilities/Tools.hpp"
#include "../renderer/FileLoader.hpp"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <system_error>
#include <unordered_map>

#ifndef ORKA_DISABLE_PHYSICS
#include "../physics/PhysicsEngine.hpp"
#endif

static std::unordered_map<std::string, std::shared_ptr<Model>>& sceneNodeModelCache() {
    static auto* cache = new std::unordered_map<std::string, std::shared_ptr<Model>>();
    return *cache;
}

static std::unordered_map<std::string, std::shared_ptr<Model>>& rawModelPathCache() {
    static auto* cache = new std::unordered_map<std::string, std::shared_ptr<Model>>();
    return *cache;
}

static std::unordered_map<Model*, std::weak_ptr<Model>>& rawModelOwners() {
    static auto* cache = new std::unordered_map<Model*, std::weak_ptr<Model>>();
    return *cache;
}

static std::string toSceneNodeCacheKey(std::string_view path) {
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) return canonical.string();
    return std::filesystem::path(path).lexically_normal().string();
}

static std::shared_ptr<Model> makeOwnedSharedModel(Model* model) {
    if (model == nullptr) return {};
    auto& owners = rawModelOwners();
    auto existing = owners.find(model);
    if (existing != owners.end()) {
        std::shared_ptr<Model> reused = existing->second.lock();
        if (reused) return reused;
    }
    std::shared_ptr<Model> shared(model);
    owners[model] = shared;
    return shared;
}

static void logModelAssetLoad(std::string_view path) {
    std::error_code ec;
    const std::uintmax_t bytes = std::filesystem::file_size(path, ec);
    const std::string filename = std::filesystem::path(path).filename().string();
    std::string message = "Loading model asset container '" + filename + "'";

    if (!ec) {
        message += " (" + MiscTools::formatByteSize(bytes) + ")";
    }

    Logger::log(LET_INFO, "Orka, ModelSceneNode", message);
}

static std::shared_ptr<Model> loadOrReuseSceneNodeModel(std::string_view sceneNodePath, std::string_view modelPath) {
    const std::string cacheKey = toSceneNodeCacheKey(sceneNodePath);
    auto& cache = sceneNodeModelCache();
    auto cacheIt = cache.find(cacheKey);
    if (cacheIt != cache.end()) {
        if (cacheIt->second) return cacheIt->second;
    }

    logModelAssetLoad(modelPath);
    std::shared_ptr<Model> loaded = makeOwnedSharedModel(FileLoader::loadGLTFModel(modelPath, true, false, true));
    cache[cacheKey] = loaded;
    return loaded;
}

static std::shared_ptr<Model> loadOrReuseRawModel(std::string_view modelPath) {
    const std::string cacheKey = toSceneNodeCacheKey(modelPath);
    auto& cache = rawModelPathCache();
    auto cacheIt = cache.find(cacheKey);
    if (cacheIt != cache.end()) {
        if (cacheIt->second) return cacheIt->second;
    }

    logModelAssetLoad(modelPath);
    std::shared_ptr<Model> loaded = makeOwnedSharedModel(FileLoader::loadGLTFModel(modelPath, true, false, true));
    cache[cacheKey] = loaded;
    return loaded;
}

void ModelSceneNode::clearModelCaches() {
    auto& sceneCache = sceneNodeModelCache();
    for (auto it = sceneCache.begin(); it != sceneCache.end(); ) {
        if (!it->second || it->second.use_count() <= 1) it = sceneCache.erase(it);
        else ++it;
    }
    auto& rawCache = rawModelPathCache();
    for (auto it = rawCache.begin(); it != rawCache.end(); ) {
        if (!it->second || it->second.use_count() <= 1) it = rawCache.erase(it);
        else ++it;
    }
    auto& owners = rawModelOwners();
    for (auto it = owners.begin(); it != owners.end(); ) {
        if (it->second.expired()) it = owners.erase(it);
        else ++it;
    }
}


static inline bool hasExplicitPhysicsBody(std::string_view bodyType) {
    return bodyType == "rigid" || bodyType == "Rigid" || bodyType == "RIGID" ||
        bodyType == "soft" || bodyType == "Soft" || bodyType == "SOFT" ||
        bodyType == "ghost" || bodyType == "Ghost" || bodyType == "GHOST";
}

void ModelSceneNode::loadFile(std::string_view scenenodeFile) {
    m_filepath = toSceneNodeCacheKey(scenenodeFile);
    WhaleMLFile db(scenenodeFile);

    std::string name = db.string("name", "!!EMPTY_NAME!!");
    std::string model = db.string("model.file", "");
    std::string body = db.string("physics.body.type", "none");
    std::string bodySettings = db.string("physics.body.settings", "");
    std::string script = db.string("script.file", "");
    std::string shaderType = db.string("shader.type", "");
    std::string shaderFile = db.string("shader.file", "");
    
    m_customUserDataString = db.string("userdata.string", "");
    m_customUserWhaleMLFile = std::make_unique<WhaleMLFile>(); 

    if (db.hasKey("userdata.file"))
        m_customUserWhaleMLFile->load(db["userdata.file"].asString());

    bool instanceScript = true;
    if (db.hasKey("script.instance"))
        instanceScript = db["script.instance"].asBool();

    m_progressID = ProgressManager::addElement("SceneNode_" + std::to_string(m_id.getValue()), { "File parsed", "Meshes/Textures loaded", "Physics ready", "Script compiled" }); // Progress steps with File Loading
    ProgressManager::step(m_progressID); // File Loaded
    m_fileLoaded = true;
    createManual(name, MiscTools::getPath(scenenodeFile), model, body, bodySettings, script, shaderType, shaderFile, instanceScript);
}
#ifdef ORKA_DISABLE_PHYSICS
void ModelSceneNode::loadGLB(std::string_view modelFile) {
#else
void ModelSceneNode::loadGLB(std::string_view modelFile, ModelSceneNodePhysicsBodyType bodyType, std::string_view bodyShape, float mass) {
#endif
#ifndef ORKA_DISABLE_PHYSICS
    const std::string resolvedBodyShape = bodyShape.empty() ? "convexhull" : std::string(bodyShape);
#endif
    const std::string rootPath = MiscTools::getPath(modelFile);
    const std::string meshFile = MiscTools::getFilename(modelFile);
    const std::string modelPath = MiscTools::resolveRelativePath(rootPath, meshFile);
    std::string bodyTypeString;
#ifndef ORKA_DISABLE_PHYSICS
    switch (bodyType) {
        case PBT_RIGID:
            bodyTypeString = "rigid";
            break;
        case PBT_SOFT:
            bodyTypeString = "soft";
            break;
        case PBT_GHOST:
                bodyTypeString = "ghost";
                break;
        default:
            bodyTypeString = "none";
            break;
    }
#endif

    m_progressID = ProgressManager::addElement("SceneNode_" + std::to_string(m_id.getValue()), {"Meshes/Textures loaded", "Physics ready", "Script compiled" });
    m_fileLoaded = true;
    m_filepath.clear();
    m_model = loadOrReuseRawModel(modelPath);
    createManual(meshFile, rootPath, meshFile, bodyTypeString, "", "", "", "", true, true, true);

    Model* modelPtr = m_model.get();
    if (modelPtr == nullptr) {
        ProgressManager::step(m_progressID); // Physics Ready (Skipped)
        SceneNode::create("", true);
        return;
    }

#ifndef ORKA_DISABLE_PHYSICS
    WhaleMLFile settings;
    if (bodyType == PBT_RIGID) {
        settings.setValue("mass", mass);
        settings.setValue("friction", 0.5);
        settings.setValue("restitution", 0.0);
        settings.setValue("collision_shape", resolvedBodyShape);
        settings.setValue("linear_damping", 0.01);
        settings.setValue("angular_damping", 0.02);
        settings.setValue("rolling_friction", 0.0);
        settings.setValue("spinning_friction", 0.0);
        settings.setValue("ccd", false);
        settings.setValue("ccd_motion_threshold", 0.0000001);
        settings.setValue("ccd_swept_sphere_radius", 0.2);
        settings.setValue("kinematic", false);
    } else if (bodyType == PBT_SOFT) {
        settings.setValue("mass", mass);
        settings.setValue("linear_stiffness", 0.5);
        settings.setValue("angular_stiffness", 1.0);
        settings.setValue("volume_stiffness", 1.0);
        settings.setValue("damping", 0.01);
        settings.setValue("friction", 0.5);
        settings.setValue("pose_matching", 0.0);
        settings.setValue("pressure", 0.0);
        settings.setValue("bending_constraints", 2);
    } else {
        settings.setValue("collision_shape", resolvedBodyShape);
    }

    m_physicsSettingsWhaleMLFile = settings;
    if (bodyType == PBT_RIGID) {
        PhysicsEngine::addRigidBody(m_id, m_physicsSettingsWhaleMLFile, modelPtr);
    } else if (bodyType == PBT_SOFT) {
        PhysicsEngine::addSoftBody(m_id, m_physicsSettingsWhaleMLFile, modelPtr, m_scale);
    } else {
        PhysicsEngine::addGhostObject(m_id, m_physicsSettingsWhaleMLFile, modelPtr);
    }
#endif

    ProgressManager::step(m_progressID); // Physics Ready
    SceneNode::create("", true);
}

void ModelSceneNode::createManual(std::string_view name, std::string_view rootpath, std::string_view mesh, std::string_view body, std::string_view bodySettings, std::string_view script, std::string_view shadertype, std::string_view shaderfile, bool instanceScript, bool skipFileLoading, bool skipPhysicsCreation) {
    setName(name);
    m_rootpath = rootpath;
    m_mesh = mesh;
    m_body = body;
    m_bodySettings = bodySettings;
    m_shaderType = shadertype;
    m_shaderFile = shaderfile;
    //m_scriptPath = script;
    const std::string scriptPath = skipFileLoading ? std::string(script) : MiscTools::resolveRelativePath(rootpath, script);

    if (!m_fileLoaded) {
        m_progressID = ProgressManager::addElement("SceneNode_" + std::to_string(m_id.getValue()), {"Meshes/Textures loaded", "Physics ready", "Script compiled" }); // Progress steps with File Loading
    }
    m_fileLoaded = true;

    if (!mesh.empty() && !skipFileLoading) {
        const std::string modelPath = MiscTools::resolveRelativePath(rootpath, mesh);
        if (!m_filepath.empty()) m_model = loadOrReuseSceneNodeModel(m_filepath, modelPath);
        else {
            logModelAssetLoad(modelPath);
            m_model = makeOwnedSharedModel(FileLoader::loadGLTFModel(modelPath, true, false, true));
        }
        if (!m_model) {
            Logger::log(LET_ERROR, "Orka, ModelSceneNode", "Failed to load model '" + modelPath + "' for SceneNode '" + std::string(getName()) + "'");
        }
    }

#ifndef ORKA_RENDERER_DISABLE_PBR
    ModelShadingMode mode = MSM_PBR;
#else
    ModelShadingMode mode = MSM_BASIC;
#endif
    if (shadertype == "pbr" || shadertype == "Pbr" || shadertype == "PBR") mode = MSM_PBR;
    else if (shadertype == "basic" || shadertype == "Basic" || shadertype == "BASIC") mode = MSM_BASIC;
    else if (shadertype == "unlit" || shadertype == "Unlit" || shadertype == "UNLIT") mode = MSM_UNLIT;
    else if (shadertype == "toon" || shadertype == "Toon" || shadertype == "TOON") mode = MSM_TOON;
    else if (shadertype == "custom" || shadertype == "Custom" || shadertype == "CUSTOM") {
        mode = MSM_CUSTOM;
        const std::string shaderPath = skipFileLoading ? std::string(shaderfile) : MiscTools::resolveRelativePath(rootpath, shaderfile);
        if (shaderPath.empty()) {
            Logger::log(LET_ERROR, "Orka, ModelSceneNode", "shader.file is required when shader.type is custom for SceneNode '" + std::string(getName()) + "'");
        } else if (m_model) {
            std::unique_ptr<Shader> shaderTemplate(FileLoader::loadShader(shaderPath));
            if (shaderTemplate) {
                // m_model is shared by every instance loaded from this .model.whaleml.
                // Keep it shared so those instances use the same material configuration.
                for (Mesh* mesh : m_model->getMeshes()) {
                    const std::vector<Material*> materials = m_model->getMaterials(mesh);
                    for (Material* material : materials) {
                        // Materials own their Shader. Do not replace an already assigned one
                        // when another SceneNode reuses the cached model.
                        if (material && material->getShader() == nullptr) {
                            material->setShader(new Shader(*shaderTemplate));
                        }
                    }
                }
            } else {
                Logger::log(LET_ERROR, "Orka, ModelSceneNode", "Failed to load custom shader '" + shaderPath + "' for SceneNode '" + std::string(getName()) + "'");
            }
        }
    }
    if (m_model) m_model->setShadingMode(mode);

    ProgressManager::step(m_progressID); // Meshes/Textures/Shader loaded

    Model* modelPtr = m_model.get();
    if (modelPtr == nullptr || skipPhysicsCreation) {
        ProgressManager::step(m_progressID); // Physics Ready (Skipped)
        SceneNode::create(scriptPath, instanceScript);
        return;
    }

#ifndef ORKA_DISABLE_PHYSICS
    if (!skipFileLoading) {
        m_physicsSettingsWhaleMLFile = WhaleMLFile();
        const std::string settingsPath = MiscTools::resolveRelativePath(rootpath, bodySettings);
        if (!settingsPath.empty()) m_physicsSettingsWhaleMLFile.load(settingsPath);
    }

    if (body == "rigid" || body == "Rigid" || body == "RIGID") { // Rigid body
        PhysicsEngine::addRigidBody(m_id, m_physicsSettingsWhaleMLFile, modelPtr);
    } else if (body == "soft" || body == "Soft" || body == "SOFT") { // Soft body
        PhysicsEngine::addSoftBody(m_id, m_physicsSettingsWhaleMLFile, modelPtr, m_scale);
    } else if (body == "ghost" || body == "Ghost" || body == "GHOST") { // Ghost object
        PhysicsEngine::addGhostObject(m_id, m_physicsSettingsWhaleMLFile, modelPtr);
    } else if (!body.empty() && body != "none" && body != "None" && body != "NONE") {
        Logger::log(LET_WARN, "Orka, ModelSceneNode, Physics", "Unknown physics.body.type '" + std::string(body) + "' for SceneNode '" + std::string(getName()) + "'.");
    }
#endif
    ProgressManager::step(m_progressID); // Physics Ready
    SceneNode::create(scriptPath, instanceScript);
}

void ModelSceneNode::setManualModel(Model* model) {
    m_model = makeOwnedSharedModel(model);
    m_filepath.clear();
    m_fileLoaded = true;
}

vector3df ModelSceneNode::getCenterPoint() const {
    if (m_model == nullptr || m_model->getMeshes().empty()) return getPosition();
    return getPosition() + getRotation() * (m_model->getAABB().getCenter() * getScale());
}

AABB ModelSceneNode::getAABB() const {
    AABB bounds;
    if (m_model == nullptr || m_model->getMeshes().empty()) {
        bounds.setStartEnd(getPosition(), getPosition());
        return bounds;
    }

    const AABB modelBounds = m_model->getAABB();
    const vector3df minEdge = modelBounds.getMinEdge();
    const vector3df maxEdge = modelBounds.getMaxEdge();
    const vector3df firstCorner = getPosition() + getRotation() * (minEdge * getScale());
    bounds.setStartEnd(firstCorner, firstCorner);
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                const vector3df corner(
                    x == 0 ? minEdge.x : maxEdge.x,
                    y == 0 ? minEdge.y : maxEdge.y,
                    z == 0 ? minEdge.z : maxEdge.z
                );
                bounds.expandToInclude(getPosition() + getRotation() * (corner * getScale()));
            }
        }
    }
    return bounds;
}

BS ModelSceneNode::getBS() const {
    BS bounds;
    if (m_model == nullptr || m_model->getMeshes().empty()) {
        bounds.set(getPosition(), 0.0f);
        return bounds;
    }

    const BS modelBounds = m_model->getBS();
    const vector3df scale = glm::abs(getScale());
    const float maxScale = std::max({scale.x, scale.y, scale.z});
    bounds.set(
        getPosition() + getRotation() * (modelBounds.getCenter() * getScale()),
        modelBounds.getRadius() * maxScale
    );
    return bounds;
}

ModelSceneNode& ModelSceneNode::clone() const {
    ModelSceneNode& newNode = CreatableSceneNode::clone();
    if (m_fileLoaded) {
        newNode.m_model = m_model;
        newNode.m_filepath = m_filepath;
        newNode.m_physicsSettingsWhaleMLFile = m_physicsSettingsWhaleMLFile;
        const bool skipPhysicsCreation = !hasExplicitPhysicsBody(m_body);
        newNode.createManual(getName(), m_rootpath, m_mesh, m_body, m_bodySettings, m_scriptPath, m_shaderType, m_shaderFile, m_instanceScript, true, skipPhysicsCreation); // Skip file loading!
    }
    return newNode;
}

