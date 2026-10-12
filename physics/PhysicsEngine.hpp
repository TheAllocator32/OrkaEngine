/*
 * File: PhysicsEngine.hpp
 *
 * The sacred PhysicsEngine... (Uses Bullet internally)
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
#include "../whaleml/WhaleMLFile.hpp"
#include "../essentials/Threading.hpp"
#include "PhysicsTypes.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include <future>

class btBroadphaseInterface;
class btCollisionDispatcher;
class btCollisionObject;
class btConstraintSolver;
class btDynamicsWorld;
class btGhostPairCallback;
class btITaskScheduler;
class btSoftBodyRigidBodyCollisionConfiguration;
class btSoftBodyWorldInfo;
class btSoftRigidDynamicsWorld;
class btTransform;
class btVector3;
class SceneManager;

// PhysicsEngine is a static class
class PhysicsEngine {
public:
    // Wait. You don't know what init does? This should actually be called by SceneManager. You may pass a filepath to a .world.whaleml
    static void init(std::string filepath = "");
    // See init. Should also be called by SceneManager
    static void deinit();
    // Creates the Physics world
    static void createPhysicsWorld(std::string filepath);
    static void createDefaultPhysicsWorld();
    // Destroys the universe!
    static void destroyPhysicsWorld();

    // Some miscellaneous Physics operations (you have to pass a SceneNode ID to them):
    // Moves a body (smoothly) in _time_ seconds to destination _newPosition_ (Only works with RigidBodies... Of course)
    static void move(Identifier sceneNode, vector3df newPosition, float time);
    // Applies a (central) impulse on a Body (Only RigidBodies supported)
    static void applyCentralImpulse(Identifier sceneNode, vector3df impulse);
    // Applies an impulse at a relative position on a Body (Only RigidBodies supported)
    static void applyImpulse(Identifier sceneNode, vector3df impulse, vector3df relativePosition);
    // Applies a torque impulse on a Body (Only RigidBodies supported)
    static void applyTorqueImpulse(Identifier sceneNode, vector3df torque);
    // Applies a rotation impulse on a Body (Only RigidBodies supported) (very weird in real world unless you want to spin fish)
    static void applyRotationImpulse(Identifier sceneNode, vector3df rotationImpulse);
    // Applies a central force on a Body (Only RigidBodies supported)
    static void applyCentralForce(Identifier sceneNode, vector3df force);
    // Applies a force at a relative position on a Body (Only RigidBodies supported)
    static void applyForce(Identifier sceneNode, vector3df force, vector3df relativePosition);
    // Applies torque on a Body (Only RigidBodies supported)
    static void applyTorque(Identifier sceneNode, vector3df torque);
    // Sets the linear velocity of a Body (Only RigidBodies supported)
    static void setLinearVelocity(Identifier sceneNode, vector3df velocity);
    // Returns the linear velocity of a Body (Only RigidBodies supported)
    static vector3df getLinearVelocity(Identifier sceneNode);
    // Sets the angular velocity of a Body (Only RigidBodies supported)
    static void setAngularVelocity(Identifier sceneNode, vector3df velocity);
    // Returns the angular velocity of a Body (Only RigidBodies supported)
    static vector3df getAngularVelocity(Identifier sceneNode);
    // Sets the mass of the body! (e.g after scaling)
    static void setMass(Identifier sceneNode, float mass);
    // Returns the mass of a Rigid or Soft Body
    static float getMass(Identifier sceneNode);
    // Returns the bare rigidBody/softBody bonded to the SceneNode
    static btCollisionObject* getBody(Identifier sceneNode);
    // Returns true if a body exists for sceneNode without warning logs
    static bool hasBody(Identifier sceneNode);

    // Collision queries (Rigid Body and Ghost Object only)
    // Adds a collision callback which will be called whenever your selected body/bodies collide.
    static Identifier addCollisionCallback(std::function<void()> callback, Identifier sceneNodeA, Identifier sceneNodeB);
    static Identifier addCollisionCallback(std::function<void(Identifier sceneNodeB)> callback, Identifier sceneNodeA);
    // Removes a(ll) collisionCallback(s)
    static void removeCollisionCallback(Identifier id);
    static void clearCollisionCallbacks();
    // A simple collision/overlap query that checks the collision between 2 objects (unfortunately not very lightweight). Again...
    static bool areColliding(Identifier a, Identifier b);
    // Returns all SceneNodes (IDs) that collide with your selected SceneNode. (Unfortunately this is quite performance intensive... 3 iterations).
    static std::vector<Identifier> collisionQuery(Identifier sceneNode);
    // Returns all active physics SceneNodes that are currently colliding with something.
    static std::vector<Identifier> getCollidingSceneNodes();
    // Returns the closest physics body hit between two world-space points
    static RaycastResult raycast(vector3df from, vector3df to);
    // Returns all physics bodies (and their individual intersections) hit between two world-space points, sorted by distance
    static std::vector<RaycastResult> raycastAll(vector3df from, vector3df to);

    // Body physics instantiation!
    // Adds a Rigid Body (Or Kinematic body)! Can be called manually but is usually done by SceneNode
    static void addRigidBody(Identifier sceneNode, std::string settingsFile, Model* mesh);
    static void addRigidBody(Identifier sceneNode, WhaleMLFile& settings, Model* mesh);
    // Adds a Soft Body... Wow.
    static void addSoftBody(Identifier sceneNode, std::string settingsFile, Model* mesh, vector3df scale=vector3df(1,1,1));
    static void addSoftBody(Identifier sceneNode, WhaleMLFile& settings, Model* mesh, vector3df scale=vector3df(1, 1, 1));
    // Adds  a Ghost Object (an object that exists but does not affect physics, can be used as trigger area etc)
    static void addGhostObject(Identifier sceneNode, std::string settingsFile, Model* mesh);
    static void addGhostObject(Identifier sceneNode, WhaleMLFile& settings, Model* mesh);
    // Queues a body for removal on the physics thread
    static void queueBodyRemoval(Identifier sceneNode);
    // Removes a Rigid/Soft body
    static void removeBody(Identifier sceneNode);
    // Removes multiple Rigid/Soft/Ghost bodies in one synchronized physics task
    static void removeBodies(const std::vector<Identifier>& sceneNodes);

    // User Options:
    // Sets the simulation speed in FPS (set to 0 or leave empty to synchronize physics simulation speed and rendering framerate)
    inline static void setSimulationSpeed(int speed = 0) { sm_simulationSpeed = speed; }
    static void pauseSimulation();
    static void resumeSimulation();
    inline static bool isSimulationPaused() { return sm_pauseRequests.load(std::memory_order_acquire) > 0; }

    // Update:
    // Updates all activated bodies and updates their position and rotation, blocks the thread to avoid 100% CPU load! -- Usually called automatically by PhysicsThread
    static void update(bool blockThread=true);

    // Threading:
    // Starts physics simulation thread
    static void startPhysicsThread();
    // Stops physics simulation thread
    static void stopPhysicsThread();
    // Returns whether current thread is the physics thread
    static bool amIPhysicsThread();
    // Returns whether the dedicated physics thread is currently running
    inline static bool isPhysicsThreadRunning() { return sm_physicsThreadRunning.load(std::memory_order_acquire); }

    // Advanced:
    // Returns the internal btSoftRigidDynamicsWorld (for advanced users)
    static btSoftRigidDynamicsWorld* getDynamicsWorld() { return sm_world; }

private:
    struct RigidTransformSnapshot {
        Identifier sceneNode;
        vector3df previousPosition = vector3df(0.0f);
        vector3df currentPosition = vector3df(0.0f);
        quaternion previousRotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
        quaternion currentRotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    };

    struct TransformFrame {
        std::vector<RigidTransformSnapshot> rigidBodies;
        std::chrono::steady_clock::time_point simulationTime;
        float fixedStep;
        bool valid;
    };

    friend class SceneManager;
    // Applies interpolated physics transforms to SceneNodes on the main thread
    static void interpolateTransforms();
    // Just sets the position of the SceneNode...
    static void setPosition(Identifier sceneNode, vector3df position, bool stopMovement=true);
    // Sets the orientation/rotation of a SceneNode. (Only RigidBodies and GhostObjects supported)
    static void setRotation(Identifier sceneNode, quaternion rotation, bool stopMovement=true);
    // Sets the orientation/rotation of a SceneNode. (Only RigidBodies and GhostObjects supported)
    static void setScale(Identifier sceneNode, vector3df scale);

    inline static void schedule(std::function<void()> task) {
        if (amIPhysicsThread()) {
            task();
            return;
        }
        if (!sm_physicsThreadRunning.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(sm_physicsMutex);
            task();
            return;
        }
        sm_physicsThreadTasks->push_back(std::move(task));
    }
    static void scheduleBlocking(std::function<void()> task);
    static void resetRigidNodeTransformSnapshotNoLock(Identifier sceneNode, const btTransform& transform);
    static void registerSceneNodePhysicsTransformCallback(Identifier sceneNode);
    static void clearSceneNodePhysicsTransformCallback(Identifier sceneNode);
    static void clearTransformFrame();
    static btCollisionObject* getBodyNoLock(Identifier sceneNode);
    static bool hasBodyNoLock(Identifier sceneNode);
    static Identifier getSceneNodeNoLock(const btCollisionObject* body);
    static bool deleteBodyNoLock(Identifier sceneNode);
    static bool internalAreCollidingNoLock(btCollisionObject* objA, btCollisionObject* objB, btDynamicsWorld* world);
    static bool areCollidingNoLock(Identifier a, Identifier b);
    static void createPhysicsWorldInternal(float gravityY, float softBodyGravityY, int solverIterations, bool splitImpulse, float airDensity, float waterDensity, float waterOffset, const btVector3& waterNormal, bool useMtDispatcher, bool useMtSolver);
private:
    inline static std::vector<RigidNode> sm_rigidNodes;
    inline static std::vector<SoftNode> sm_softNodes;
    inline static std::vector<GhostObject> sm_ghostObjects;
    inline static btSoftRigidDynamicsWorld* sm_world;
    inline static btBroadphaseInterface* sm_broadphase;
    inline static btCollisionDispatcher* sm_dispatcher;
    inline static btSoftBodyRigidBodyCollisionConfiguration* sm_collisionConfig;
    inline static btConstraintSolver* sm_solver;
    inline static btSoftBodyWorldInfo* sm_softBodyWorldInfo;
    inline static btGhostPairCallback* sm_ghostPairCallback = nullptr;
    inline static btITaskScheduler* sm_taskScheduler = nullptr;
    inline static int sm_simulationSpeed = 0;
    inline static std::unordered_map<Identifier, CollisionCallback> sm_collisionCallbacks;
    inline static Identifier sm_collisionCallbackIDCounter = 0;
    inline static std::unordered_map<Identifier, MovementTask> sm_moveTasks;
    inline static std::atomic_bool sm_physicsThreadRunning = false;
    inline static std::atomic_int sm_pauseRequests = 0;
    inline static threadsafe<std::vector<std::function<void()>>> sm_physicsThreadTasks;
    inline static std::thread sm_physicsThread;
    inline static threadsafe<std::thread::id> sm_physicsThreadID;
    inline static std::mutex sm_physicsMutex;
    inline static std::mutex sm_transformFrameMutex;
    inline static TransformFrame sm_transformFrame{{}, {}, 1.0f / 60.0f, false};
    inline static bool sm_hasLastUpdateTime = false;
    inline static std::chrono::steady_clock::time_point sm_lastUpdateTime;
};

#endif
