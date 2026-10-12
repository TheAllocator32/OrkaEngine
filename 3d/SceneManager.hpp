/*
 * File: SceneManager.hpp
 *
 * Controlls the SceneNodes and updates them
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "SceneNode.hpp"
#include "CameraSceneNode.hpp"
#include "OrbitCameraSceneNode.hpp"
#include "ThirdPersonCameraSceneNode.hpp"
#include "../essentials/Threading.hpp"
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

class PhysicsEngine;
class Renderer;
class Scene;
class OrkaEngine;

class SceneManager {
public:
    // Creates the SceneManager, wow.
    static void create();
    // Deinitializes the SceneManager, deletes all SceneNodes
    static void clear();
    // Calls remove and then create to clear all 3d Objects on screen!
    inline static void recreate() { clear(); create(); }

    // Returns next ID (e.g for manual SceneNodes)
    static Identifier createID();

    // Returns whether a SceneNode is assigned to the ID
    static bool sceneNodeExists(Identifier ID);
    // Returns the SceneNode assigned to the ID
    static SceneNode& getSceneNode(Identifier ID);

    // Returns the full array of all SceneNodes (their IDs of course)
    static std::vector<Identifier> getAllSceneNodes();
    // Returns IDs for all SceneNodes accepted by the predicate
    static std::vector<Identifier> getSceneNodesWhere(const std::function<bool(const SceneNode&)>& predicate);

    // Returns whether a Scene is assigned to the ID.
    static bool sceneExists(Identifier ID);
    // Returns the Scene assigned to the ID.
    static Scene& getScene(Identifier ID);
    // Returns IDs for all Scenes owned by SceneManager.
    static std::vector<Identifier> getAllScenes();

    // Returns the Camera
    static CameraSceneNode& getCamera();
    // Returns true if a Camera is assigned
    inline static bool hasCamera() { return sm_camera != nullptr; }

    // Update (DO NOT CALL MANUALLY IF USING Orka.hpp)
    static void update(float delta);

    // If the SceneManager is initialized this returns true
    inline static bool isActive() { return sm_isActive; }
private:
    friend class OrkaEngine;
    friend class SceneNode;
    friend class CameraSceneNode;
    friend class OrbitCameraSceneNode;
    friend class ThirdPersonCameraSceneNode;
    friend class PhysicsEngine;
    friend class Renderer;
    friend class Scene;
    // Lets the SceneManager own a SceneNode. Automatically done by SceneNode if not disabled manually.
    static void addSceneNode(SceneNode* node);
    // Lets the SceneManager own a Scene.
    static void addScene(Scene* scene);
    static void setCamera(CameraSceneNode* camera);
    static void removeCamera(CameraSceneNode* camera);
    static void queueCameraDestroy(CameraSceneNode* camera);
    static void queueSceneNodeDestroy(Identifier ID);
    static void flushDestroyedSceneNodes();
    static void updateModelCacheCleanup(float delta);
    static void removeSceneNode(Identifier ID);
private:
    inline static threadsafe<std::unordered_map<Identifier, SceneNode*>> sm_sceneNodes;
    inline static threadsafe<std::unordered_map<Identifier, Scene*>> sm_scenes;
    inline static threadsafe<std::vector<Identifier>> sm_destroySceneNodeQueue;
    inline static threadsafe<std::vector<CameraSceneNode*>> sm_destroyCameraQueue;
    inline static CameraSceneNode* sm_camera;
    inline static threadsafe<Identifier> sm_IDCount;
    inline static bool sm_isActive;
    inline static float sm_modelCacheIdleSeconds;
    inline static bool sm_modelCachesClearedAfterIdle;
};
