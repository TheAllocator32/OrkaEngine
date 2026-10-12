/*
 * File: PhysicsTypes.hpp
 *
 * Contains structs/classes/enums used by Orka's Physics section
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#ifndef ORKA_DISABLE_PHYSICS
#include <functional>
#include <memory>
#include "../primitives/core/MathTypes.hpp"
#include "../essentials/Identifier.hpp"

class Model;
class btCollisionShape;
class btGhostObject;
class btRigidBody;
class btSoftBody;
class btTriangleMesh;

// Collision shape/type
enum CollisionShape {
    CT_NONE,                // Does not exist in this universe but in yours? Maybe!
    CT_AABB,                // Axis Aligned Bounding Box "classic hitbox"
    CT_BS,                    // Bounding Sphere (ball). Will be able to roll!
    CT_CONVEXHULL,            // A single convex hull
    CT_CONVEXDECOMPOSITION,    // Convex Decomposition (multiple convex hulls put together) aka. "Convex Hull for Worldmaps"
    CT_MESH,                // Uses the exact mesh as collision shape! (Very CPU intensive and really really not recommended either you know what you are doing or benchmarking)
    CT_STATICMESH            // Same as MESH but optimized for WorldMaps/Levels! Much less CPU intensive but static (no movement!)
};

struct RigidNode {
    btRigidBody* body;
    Identifier sceneNode;
    Model* mesh; // We call it mesh for backwards compatibility but it's actually a Model :/
    std::shared_ptr<btCollisionShape> shapeOwner;
    std::shared_ptr<btTriangleMesh> triangleMeshOwner; // Only used by mesh/staticmesh/trimesh collision shapes
    vector3df previousPosition = vector3df(0.0f);
    vector3df currentPosition = vector3df(0.0f);
    quaternion previousRotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    quaternion currentRotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    bool hasTransformSnapshot = false;
    bool active = true;
};

struct SoftNode {
    btSoftBody* body;
    Identifier sceneNode;
    Model* mesh;
    bool active = true;
};

struct GhostObject {
    btGhostObject* body;
    Identifier sceneNode;
    Model* mesh;
    std::shared_ptr<btCollisionShape> shapeOwner;
    std::shared_ptr<btTriangleMesh> triangleMeshOwner; // Only used by mesh/staticmesh/trimesh collision shapes
    bool active = true;
};

struct CollisionCallback {
    std::function<void()> callback;
    std::function<void(Identifier sceneNodeB)> altCallback;
    Identifier nodeA, nodeB;
};

struct RaycastResult {
    // True if the ray hit any physics body.
    bool hit = false;
    // SceneNode ID of the hit body, or -1 if no matching SceneNode was found.
    Identifier sceneNode;
    // World-space position where the ray hit.
    vector3df hitPoint = vector3df(0, 0, 0);
    // World-space surface normal at the hit point.
    vector3df hitNormal = vector3df(0, 0, 0);
    // Normalized distance from ray start to hit point in the range 0..1.
    float hitFraction = 1.0f;
};

struct MovementTask {
    Identifier sceneNode;
    vector3df startPos;
    vector3df targetPos;
    float totalTime;
    float elapsedTime;
};

#endif

