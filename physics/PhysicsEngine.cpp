/*
 * File: PhysicsEngine.cpp
 *
 * The PhysicsEngine's implementation (Uses Bullet internally)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#ifndef ORKA_DISABLE_PHYSICS
#include "PhysicsEngine.hpp"
#include "../3d/SceneNode.hpp"
#include "../3d/SceneManager.hpp"
#include "../primitives/3d/Model.hpp"
#include "../essentials/Threading.hpp"
#include "../essentials/Timing.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <ConvexDecomposition/ConvexBuilder.h>
#include <LinearMath/btThreads.h>
#include "../utilities/Tools.hpp"
#include "../essentials/Logger.hpp"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <utility>
#include <memory>
#include <unordered_map>
#include <iomanip>
#include <sstream>
#include <future>
#include <vector>

#define BEGINLAMBDA PhysicsEngine::schedule([=]() {
#define ENDLAMBDA });

using CollisionShapePtr = std::shared_ptr<btCollisionShape>;
using TriangleMeshPtr = std::shared_ptr<btTriangleMesh>;

static std::unordered_map<std::string, std::weak_ptr<btCollisionShape>> sg_collisionShapeCache;
static std::unordered_map<std::string, std::weak_ptr<btTriangleMesh>> sg_triangleMeshCache;

void PhysicsEngine::createPhysicsWorldInternal(float gravityY, float softBodyGravityY, int solverIterations, bool splitImpulse, float airDensity, float waterDensity, float waterOffset, const btVector3& waterNormal, bool useMtDispatcher, bool useMtSolver) {
    sm_hasLastUpdateTime = false;
    sm_collisionConfig = new btSoftBodyRigidBodyCollisionConfiguration();
    sm_dispatcher = useMtDispatcher
        ? static_cast<btCollisionDispatcher*>(new btCollisionDispatcherMt(sm_collisionConfig))
        : new btCollisionDispatcher(sm_collisionConfig);
    sm_broadphase = new btDbvtBroadphase();
    sm_solver = useMtSolver
        ? static_cast<btConstraintSolver*>(new btSequentialImpulseConstraintSolverMt())
        : static_cast<btConstraintSolver*>(new btSequentialImpulseConstraintSolver());

    sm_world = new btSoftRigidDynamicsWorld(
        sm_dispatcher,
        sm_broadphase,
        sm_solver,
        sm_collisionConfig
    );

    sm_world->setGravity(btVector3(0, gravityY, 0));
    sm_world->getSolverInfo().m_numIterations = solverIterations;
    sm_world->getSolverInfo().m_splitImpulse = splitImpulse;

    sm_softBodyWorldInfo = new btSoftBodyWorldInfo();
    sm_softBodyWorldInfo->m_broadphase = sm_broadphase;
    sm_softBodyWorldInfo->m_dispatcher = sm_dispatcher;
    sm_softBodyWorldInfo->m_gravity = btVector3(0, softBodyGravityY, 0);
    sm_softBodyWorldInfo->air_density = airDensity;
    sm_softBodyWorldInfo->water_density = waterDensity;
    sm_softBodyWorldInfo->water_offset = waterOffset;
    sm_softBodyWorldInfo->water_normal = waterNormal;
    sm_softBodyWorldInfo->m_sparsesdf.Initialize();
    btGImpactCollisionAlgorithm::registerAlgorithm(sm_dispatcher);
    if (sm_ghostPairCallback) {
        delete sm_ghostPairCallback;
        sm_ghostPairCallback = nullptr;
    }
    sm_ghostPairCallback = new btGhostPairCallback();
    sm_world->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(sm_ghostPairCallback);
}

static std::string formatFloatKey(float value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << value;
    return oss.str();
}

static std::string formatVector3Key(const vector3df& value) {
    return formatFloatKey(value.x) + "," + formatFloatKey(value.y) + "," + formatFloatKey(value.z);
}

static std::string formatPointerKey(const void* ptr) {
    std::ostringstream oss;
    oss << ptr;
    return oss.str();
}

static bool readVector3Setting(WhaleMLFile& db, std::string_view key, float& x, float& y, float& z) {
    bool success = false;
    vector3df value = db.vector3d(key, vector3df(0.0f, 0.0f, 0.0f), success);
    if (!success)
        return false;
    x = value.x;
    y = value.y;
    z = value.z;
    return true;
}

static bool isConvexDecompositionShape(std::string_view shapeType) {
    return shapeType == "convexdecomposition" ||
           shapeType == "convex_decomposition" ||
           shapeType == "convexdecomp";
}

void PhysicsEngine::scheduleBlocking(std::function<void()> task) {
    if (amIPhysicsThread()) {
        task();
        return;
    }

    if (!sm_physicsThreadRunning.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(sm_physicsMutex);
        task();
        return;
    }

    auto promise = std::make_shared<std::promise<void>>();
    std::future<void> finished = promise->get_future();
    sm_physicsThreadTasks->push_back([task = std::move(task), promise]() mutable {
        task();
        promise->set_value();
    });
    finished.wait();
}

void PhysicsEngine::resetRigidNodeTransformSnapshotNoLock(Identifier sceneNode, const btTransform& transform) {
    const btVector3 position = transform.getOrigin();
    const btQuaternion rotation = transform.getRotation();
    for (auto& rigidNode : sm_rigidNodes) {
        if (rigidNode.sceneNode != sceneNode) continue;
        rigidNode.previousPosition = vector3df(position.x(), position.y(), position.z());
        rigidNode.currentPosition = rigidNode.previousPosition;
        rigidNode.previousRotation = quaternion(rotation.w(), rotation.x(), rotation.y(), rotation.z());
        rigidNode.currentRotation = rigidNode.previousRotation;
        rigidNode.hasTransformSnapshot = true;
        return;
    }
}

void PhysicsEngine::clearTransformFrame() {
    std::lock_guard<std::mutex> lock(sm_transformFrameMutex);
    sm_transformFrame = TransformFrame{};
}

void PhysicsEngine::registerSceneNodePhysicsTransformCallback(Identifier sceneNode) {
    auto sceneNodes = SceneManager::sm_sceneNodes.get();
    auto it = sceneNodes->find(sceneNode);
    if (it == sceneNodes->end() || !it->second) return;

    it->second->setTransformChangedCallback([sceneNode]() {
        auto sceneNodes = SceneManager::sm_sceneNodes.get();
        auto it = sceneNodes->find(sceneNode);
        if (it == sceneNodes->end() || !it->second) return;
        SceneNode* node = it->second;
        const vector3df position = node->getPosition();
        const quaternion rotation = node->getRotation();
        const vector3df scale = node->getScale();
        sceneNodes.unlock();

        PhysicsEngine::schedule([sceneNode, position, rotation, scale]() {
            btCollisionObject* body = PhysicsEngine::getBodyNoLock(sceneNode);
            if (!body) return;

            const btVector3 targetPosition(position.x, position.y, position.z);
            const btQuaternion targetRotation(rotation.x, rotation.y, rotation.z, rotation.w);
            const btVector3 targetScale(scale.x, scale.y, scale.z);

            if (body->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
                btRigidBody* rigidBody = btRigidBody::upcast(body);
                if (!rigidBody) return;

                btTransform trans = rigidBody->getWorldTransform();
                trans.setOrigin(targetPosition);
                trans.setRotation(targetRotation);
                rigidBody->setWorldTransform(trans);
                if (rigidBody->getMotionState())
                    rigidBody->getMotionState()->setWorldTransform(trans);
                PhysicsEngine::resetRigidNodeTransformSnapshotNoLock(sceneNode, trans);
            } else if (body->getInternalType() == btCollisionObject::CO_GHOST_OBJECT) {
                btGhostObject* ghostObject = btGhostObject::upcast(body);
                if (!ghostObject) return;

                btTransform trans = ghostObject->getWorldTransform();
                trans.setOrigin(targetPosition);
                trans.setRotation(targetRotation);
                ghostObject->setWorldTransform(trans);
            } else if (body->getInternalType() == btCollisionObject::CO_SOFT_BODY) {
                btSoftBody* softBody = btSoftBody::upcast(body);
                if (!softBody) return;

                btVector3 center(0, 0, 0);
                const int count = softBody->m_nodes.size();
                for (int i = 0; i < count; ++i)
                    center += softBody->m_nodes[i].m_x;
                if (count > 0)
                    center /= (btScalar)count;
                const btVector3 delta = targetPosition - center;

                for (int i = 0; i < count; ++i) {
                    softBody->m_nodes[i].m_x += delta;
                    softBody->m_nodes[i].m_q += delta;
                }
                return;
            }

            btCollisionShape* shape = body->getCollisionShape();
            if (!shape) return;
            const btVector3 currentScale = shape->getLocalScaling();
            if (currentScale.x() == targetScale.x() &&
                currentScale.y() == targetScale.y() &&
                currentScale.z() == targetScale.z()) {
                return;
            }

            PhysicsEngine::setScale(sceneNode, scale);
            if (body->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
                btRigidBody* rigidBody = btRigidBody::upcast(body);
                if (rigidBody) {
                    rigidBody->activate(true);
                }
            }
        });
    });
}

void PhysicsEngine::clearSceneNodePhysicsTransformCallback(Identifier sceneNode) {
    auto sceneNodes = SceneManager::sm_sceneNodes.get();
    auto it = sceneNodes->find(sceneNode);
    if (it == sceneNodes->end() || !it->second) return;
    it->second->setTransformChangedCallback(nullptr);
}

void PhysicsEngine::queueBodyRemoval(Identifier sceneNode) {
    schedule([=]() {
        deleteBodyNoLock(sceneNode);
    });
}

template <typename Builder>
static CollisionShapePtr getOrCreateCollisionShape(const std::string& key, Builder&& builder) {
    auto cached = sg_collisionShapeCache.find(key);
    if (cached != sg_collisionShapeCache.end()) {
        CollisionShapePtr existing = cached->second.lock();
        if (existing) {
            return existing;
        }
    }

    CollisionShapePtr created = builder();
    if (created) {
        sg_collisionShapeCache[key] = created;
    }
    return created;
}

template <typename Builder>
static TriangleMeshPtr getOrCreateTriangleMesh(const std::string& key, Builder&& builder) {
    auto cached = sg_triangleMeshCache.find(key);
    if (cached != sg_triangleMeshCache.end()) {
        TriangleMeshPtr existing = cached->second.lock();
        if (existing) {
            return existing;
        }
    }

    TriangleMeshPtr created = builder();
    if (created) {
        sg_triangleMeshCache[key] = created;
    }
    return created;
}

static TriangleMeshPtr createTriangleMesh(Model* mesh, const vector3df& scale) {
    if (!mesh) return {};

    TriangleMeshPtr triMesh(new btTriangleMesh(), [](btTriangleMesh* tm) { delete tm; });
    bool hasTriangles = false;
    for (Mesh* m : mesh->getMeshes()) {
        if (!m) continue;

        auto& indices = m->getIndexBuffer();
        auto& vertices = m->getVertexBuffer();
        const size_t vertexCount = vertices.size();
        const size_t indexCount = indices.size();
        if (vertexCount == 0 || indexCount < 3) continue;

        for (size_t i = 0; i + 2 < indexCount; i += 3) {
            const int i0 = static_cast<int>(indices[i]);
            const int i1 = static_cast<int>(indices[i + 1]);
            const int i2 = static_cast<int>(indices[i + 2]);

            if (i0 < 0 || i1 < 0 || i2 < 0) continue;
            if (static_cast<size_t>(i0) >= vertexCount ||
                static_cast<size_t>(i1) >= vertexCount ||
                static_cast<size_t>(i2) >= vertexCount) continue;

            const vector3df& p0 = vertices[i0].position;
            const vector3df& p1 = vertices[i1].position;
            const vector3df& p2 = vertices[i2].position;

            triMesh->addTriangle(
                btVector3(p0.x * scale.x, p0.y * scale.y, p0.z * scale.z),
                btVector3(p1.x * scale.x, p1.y * scale.y, p1.z * scale.z),
                btVector3(p2.x * scale.x, p2.y * scale.y, p2.z * scale.z));
            hasTriangles = true;
        }
    }
    return hasTriangles ? triMesh : TriangleMeshPtr{};
}

static void deleteCompoundShapeWithChildren(btCollisionShape* shape) {
    auto* compound = static_cast<btCompoundShape*>(shape);
    for (int i = 0; i < compound->getNumChildShapes(); ++i) {
        delete compound->getChildShape(i);
    }
    delete compound;
}

static CollisionShapePtr createConvexHullShape(Model* mesh, const vector3df& scale) {
    if (!mesh) return {};

    auto* hullShape = new btConvexHullShape();
    int pointCount = 0;
    for (Mesh* m : mesh->getMeshes()) {
        if (!m) continue;
        auto& vertices = m->getVertexBuffer();
        for (size_t i = 0; i < vertices.size(); ++i) {
            const vector3df& p = vertices[i].position;
            hullShape->addPoint(btVector3(p.x * scale.x, p.y * scale.y, p.z * scale.z), false);
            ++pointCount;
        }
    }

    if (pointCount == 0) {
        delete hullShape;
        return {};
    }

    hullShape->recalcLocalAabb();
    return CollisionShapePtr(hullShape, [](btCollisionShape* s) { delete s; });
}

class BulletConvexDecompositionCollector : public ConvexDecomposition::ConvexDecompInterface {
public:
    explicit BulletConvexDecompositionCollector(btCompoundShape* compoundShape)
        : m_compoundShape(compoundShape) {}

    void ConvexDecompResult(ConvexDecomposition::ConvexResult& result) override {
        if (!m_compoundShape || result.mHullVcount == 0 || !result.mHullVertices) return;

        btVector3 centroid(0, 0, 0);
        for (unsigned int i = 0; i < result.mHullVcount; ++i) {
            centroid += btVector3(
                result.mHullVertices[i * 3],
                result.mHullVertices[i * 3 + 1],
                result.mHullVertices[i * 3 + 2]);
        }
        centroid /= static_cast<btScalar>(result.mHullVcount);

        std::vector<btVector3> vertices;
        vertices.reserve(result.mHullVcount);
        for (unsigned int i = 0; i < result.mHullVcount; ++i) {
            vertices.emplace_back(
                result.mHullVertices[i * 3],
                result.mHullVertices[i * 3 + 1],
                result.mHullVertices[i * 3 + 2]);
            vertices.back() -= centroid;
        }

        auto* hullShape = new btConvexHullShape(&(vertices[0].getX()), static_cast<int>(vertices.size()), sizeof(btVector3));
        hullShape->recalcLocalAabb();

        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(centroid);
        m_compoundShape->addChildShape(transform, hullShape);
    }

private:
    btCompoundShape* m_compoundShape;
};

static CollisionShapePtr createConvexDecompositionShape(Model* mesh, const vector3df& scale) {
    if (!mesh) return {};

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vertexOffset = 0;
    for (Mesh* m : mesh->getMeshes()) {
        if (!m) continue;

        auto& meshVertices = m->getVertexBuffer();
        auto& meshIndices = m->getIndexBuffer();
        const size_t vertexCount = meshVertices.size();
        if (vertexCount == 0 || meshIndices.size() < 3) {
            vertexOffset += static_cast<unsigned int>(vertexCount);
            continue;
        }

        vertices.reserve(vertices.size() + vertexCount * 3);
        for (size_t i = 0; i < vertexCount; ++i) {
            const vector3df& p = meshVertices[i].position;
            vertices.push_back(p.x * scale.x);
            vertices.push_back(p.y * scale.y);
            vertices.push_back(p.z * scale.z);
        }

        for (size_t i = 0; i + 2 < meshIndices.size(); i += 3) {
            const unsigned int i0 = static_cast<unsigned int>(meshIndices[i]);
            const unsigned int i1 = static_cast<unsigned int>(meshIndices[i + 1]);
            const unsigned int i2 = static_cast<unsigned int>(meshIndices[i + 2]);
            if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) continue;

            indices.push_back(vertexOffset + i0);
            indices.push_back(vertexOffset + i1);
            indices.push_back(vertexOffset + i2);
        }

        vertexOffset += static_cast<unsigned int>(vertexCount);
    }

    if (vertices.empty() || indices.size() < 3) {
        return {};
    }

    auto* compoundShape = new btCompoundShape(true);
    BulletConvexDecompositionCollector collector(compoundShape);

    ConvexDecomposition::DecompDesc desc;
    desc.mVcount = static_cast<unsigned int>(vertices.size() / 3);
    desc.mVertices = vertices.data();
    desc.mTcount = static_cast<unsigned int>(indices.size() / 3);
    desc.mIndices = indices.data();
    desc.mDepth = 5;
    desc.mCpercent = 5.0f;
    desc.mPpercent = 15.0f;
    desc.mMaxVertices = 16;
    desc.mSkinWidth = 0.0f;
    desc.mCallback = &collector;

    ConvexBuilder builder(desc.mCallback);
    builder.process(desc);

    if (compoundShape->getNumChildShapes() == 0) {
        delete compoundShape;
        return createConvexHullShape(mesh, scale);
    }

    compoundShape->recalculateLocalAabb();
    return CollisionShapePtr(compoundShape, deleteCompoundShapeWithChildren);
}

static CollisionShapePtr cloneCollisionShape(const btCollisionShape* shape, const TriangleMeshPtr& triangleMeshOwner) {
    if (!shape) return {};

    CollisionShapePtr cloned;
    switch (shape->getShapeType()) {
        case BOX_SHAPE_PROXYTYPE: {
            auto* box = static_cast<const btBoxShape*>(shape);
            cloned.reset(new btBoxShape(box->getHalfExtentsWithoutMargin()), [](btCollisionShape* s) { delete s; });
            break;
        }
        case SPHERE_SHAPE_PROXYTYPE: {
            auto* sphere = static_cast<const btSphereShape*>(shape);
            cloned.reset(new btSphereShape(sphere->getRadius()), [](btCollisionShape* s) { delete s; });
            break;
        }
        case CAPSULE_SHAPE_PROXYTYPE: {
            auto* capsule = static_cast<const btCapsuleShape*>(shape);
            cloned.reset(new btCapsuleShape(capsule->getRadius(), capsule->getHalfHeight() * 2.0f), [](btCollisionShape* s) { delete s; });
            break;
        }
        case CONVEX_HULL_SHAPE_PROXYTYPE: {
            auto* sourceHull = static_cast<const btConvexHullShape*>(shape);
            if (sourceHull->getNumPoints() == 0) return {};
            auto* targetHull = new btConvexHullShape();
            const btVector3* points = sourceHull->getUnscaledPoints();
            for (int i = 0; i < sourceHull->getNumPoints(); ++i) {
                targetHull->addPoint(points[i], false);
            }
            targetHull->recalcLocalAabb();
            cloned.reset(targetHull, [](btCollisionShape* s) { delete s; });
            break;
        }
        case COMPOUND_SHAPE_PROXYTYPE: {
            auto* sourceCompound = static_cast<const btCompoundShape*>(shape);
            auto* targetCompound = new btCompoundShape(true, sourceCompound->getNumChildShapes());
            for (int i = 0; i < sourceCompound->getNumChildShapes(); ++i) {
                const btCollisionShape* sourceChild = sourceCompound->getChildShape(i);
                if (!sourceChild) continue;

                btCollisionShape* targetChild = nullptr;
                if (sourceChild->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE) {
                    auto* sourceHull = static_cast<const btConvexHullShape*>(sourceChild);
                    auto* targetHull = new btConvexHullShape();
                    const btVector3* points = sourceHull->getUnscaledPoints();
                    for (int p = 0; p < sourceHull->getNumPoints(); ++p) {
                        targetHull->addPoint(points[p], false);
                    }
                    targetHull->recalcLocalAabb();
                    targetChild = targetHull;
                } else {
                    deleteCompoundShapeWithChildren(targetCompound);
                    return {};
                }

                targetChild->setMargin(sourceChild->getMargin());
                targetChild->setLocalScaling(sourceChild->getLocalScaling());
                targetCompound->addChildShape(sourceCompound->getChildTransform(i), targetChild);
            }
            targetCompound->recalculateLocalAabb();
            cloned.reset(targetCompound, deleteCompoundShapeWithChildren);
            break;
        }
        case TRIANGLE_MESH_SHAPE_PROXYTYPE: {
            if (!triangleMeshOwner) return {};
            cloned.reset(new btBvhTriangleMeshShape(triangleMeshOwner.get(), true, true), [](btCollisionShape* s) { delete s; });
            break;
        }
        case GIMPACT_SHAPE_PROXYTYPE: {
            if (!triangleMeshOwner) return {};
            auto* gimpact = new btGImpactMeshShape(triangleMeshOwner.get());
            gimpact->updateBound();
            cloned.reset(gimpact, [](btCollisionShape* s) { delete s; });
            break;
        }
        default:
            return {};
    }

    cloned->setMargin(shape->getMargin());
    cloned->setLocalScaling(shape->getLocalScaling());
    if (cloned->getShapeType() == GIMPACT_SHAPE_PROXYTYPE) {
        static_cast<btGImpactMeshShape*>(cloned.get())->updateBound();
    }
    return cloned;
}

void PhysicsEngine::addRigidBody(Identifier sceneNode, std::string settingsFile, Model* mesh) {
    WhaleMLFile db; // WhaleML mode
    if (!settingsFile.empty()) db.load(settingsFile);
    addRigidBody(sceneNode, db, mesh);
}

void PhysicsEngine::addRigidBody(Identifier sceneNode, WhaleMLFile& settings, Model* mesh) {
    BEGINLAMBDA
    WhaleMLFile settingsMutable = settings;
    float mass = settings.floating("mass", 1.0f);
    float friction = settings.floating("friction", 0.5f);
    float restitution = settings.floating("restitution", 0.0f);
    std::string shapeType = settings.string("collision_shape", "box");

    CollisionShapePtr collisionShapeOwner;
    TriangleMeshPtr triangleMeshOwner;
    vector3df pos;
    quaternion rot;
    vector3df scale;
    auto sceneNodes = SceneManager::sm_sceneNodes.get();
    auto it = sceneNodes->find(sceneNode);
    if (it == sceneNodes->end() || !it->second || it->second->isDestroyPending()) {
        Logger::log(LET_WARN, "Orka, Physics Engine", "Cannot add rigid body: SceneNode " + std::to_string(sceneNode.getValue()) + " no longer exists.");
        return;
    }
    pos = it->second->getPosition();
    rot = it->second->getRotation();
    scale = it->second->getScale();
    sceneNodes.unlock();

    if (shapeType == "box") {
        float boxX = 1.0f, boxY = 1.0f, boxZ = 1.0f;
        if (!readVector3Setting(settingsMutable, "box_size", boxX, boxY, boxZ)) {
            if (!mesh) {
                Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot calculate box size automatically: SceneNode has no mesh.");
                return;
            }
            AABB aabb = mesh->getAABB();
            vector3df boxSize = aabb.getSize() * scale;
            boxX = boxSize.x;
            boxY = boxSize.y;
            boxZ = boxSize.z;
        }
        const std::string shapeKey = "rigid|box|" + formatFloatKey(boxX) + "|" + formatFloatKey(boxY) + "|" + formatFloatKey(boxZ);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return CollisionShapePtr(new btBoxShape(btVector3(boxX / 2.0f, boxY / 2.0f, boxZ / 2.0f)), [](btCollisionShape* s) { delete s; });
        });
    } else if (shapeType == "sphere") {
        float radius = 0.5f;
        if (settings.hasKey("sphere_radius")) {
            radius = settings["sphere_radius"].asFloat();
        } else {
            if (!mesh) {
                Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot calculate sphere radius automatically: SceneNode has no mesh.");
                return;
            }
            radius = mesh->getBS().getRadius() * scale.length();
        }
        const std::string shapeKey = "rigid|sphere|" + formatFloatKey(radius);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return CollisionShapePtr(new btSphereShape(radius), [](btCollisionShape* s) { delete s; });
        });
    } else if (shapeType == "capsule") {
        float radius = settings.floating("capsule_radius", 0.5f);
        float height = settings.floating("capsule_height", 1.0f);
        const std::string shapeKey = "rigid|capsule|" + formatFloatKey(radius) + "|" + formatFloatKey(height);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return CollisionShapePtr(new btCapsuleShape(radius, height), [](btCollisionShape* s) { delete s; });
        });
    } else if (shapeType == "convexhull") {
        if (!mesh) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "ConvexHull shape requested, but this SceneNode has no mesh! WTF??");
            return; 
        }
        const std::string shapeKey = "rigid|convexhull|mesh=" + formatPointerKey(mesh) + "|scale=" + formatVector3Key(scale);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return createConvexHullShape(mesh, scale);
        });
    } else if (isConvexDecompositionShape(shapeType)) {
        if (!mesh) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "ConvexDecomposition shape requested, but this SceneNode has no mesh.");
            return;
        }
        const std::string shapeKey = "rigid|convexdecomposition|mesh=" + formatPointerKey(mesh) + "|scale=" + formatVector3Key(scale);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return createConvexDecompositionShape(mesh, scale);
        });
    } else if (shapeType == "mesh" || shapeType == "staticmesh" || shapeType == "trimesh") {
        if (!mesh) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "Mesh shape obviously not available for this SceneNode without mesh!");
            return;
        }
        const std::string triangleMeshKey = "trimesh|mesh=" + formatPointerKey(mesh) + "|scale=" + formatVector3Key(scale);
        triangleMeshOwner = getOrCreateTriangleMesh(triangleMeshKey, [&]() {
            return createTriangleMesh(mesh, scale);
        });
        if (!triangleMeshOwner) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "Failed to create triangle mesh collision data.");
            return;
        }

        const bool useStaticMeshShape = (shapeType == "staticmesh" || mass == 0.0f);
        const std::string shapeKey = std::string("rigid|") + (useStaticMeshShape ? "bvh|" : "gimpact|") + triangleMeshKey;
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            if (useStaticMeshShape) {
                return CollisionShapePtr(new btBvhTriangleMeshShape(triangleMeshOwner.get(), true, true), [](btCollisionShape* s) { delete s; });
            }
            auto* gimpact = new btGImpactMeshShape(triangleMeshOwner.get());
            gimpact->updateBound();
            return CollisionShapePtr(gimpact, [](btCollisionShape* s) { delete s; });
        });
    } else {
        collisionShapeOwner = getOrCreateCollisionShape("rigid|fallback_box", [&]() {
            return CollisionShapePtr(new btBoxShape(btVector3(0.5f, 0.5f, 0.5f)), [](btCollisionShape* s) { delete s; });
        });
    }

    btCollisionShape* collisionShape = collisionShapeOwner.get();
    if (!collisionShape) {
        Logger::log(LET_ERROR, "Orka, Physics Engine", "Failed to create collision shape for rigid body.");
        return;
    }

    btVector3 inertia(0, 0, 0);
    if (mass > 0.0f) {
        collisionShape->calculateLocalInertia(mass, inertia);
    }

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(pos.x, pos.y, pos.z));
    startTransform.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));

    btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, collisionShape, inertia);
    rbInfo.m_friction = friction;
    rbInfo.m_restitution = restitution;
    btRigidBody* rigidBody = new btRigidBody(rbInfo);

    if (settings.hasKey("linear_damping"))
        rigidBody->setDamping(settings["linear_damping"].asFloat(), rigidBody->getAngularDamping());
    if (settings.hasKey("angular_damping"))
        rigidBody->setDamping(rigidBody->getLinearDamping(), settings["angular_damping"].asFloat());
    if (settings.hasKey("rolling_friction"))
        rigidBody->setRollingFriction(settings["rolling_friction"].asFloat());
    if (settings.hasKey("spinning_friction"))
        rigidBody->setSpinningFriction(settings["spinning_friction"].asFloat());
    if (settings.hasKey("ccd") && settings["ccd"].asBool()) {
        float ccdMotionThreshold = settings.floating("ccd_motion_threshold", 1e-7f);
        float ccdSweptSphereRadius = settings.floating("ccd_swept_sphere_radius", 0.2f);
        rigidBody->setCcdMotionThreshold(ccdMotionThreshold);
        rigidBody->setCcdSweptSphereRadius(ccdSweptSphereRadius);
    }

    if (settings.hasKey("kinematic") && settings["kinematic"].asBool()) 
        rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);

    if (sm_world == nullptr) {
        Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot add rigid body: physics world not initialized yet.");
        delete rigidBody->getMotionState();
        delete rigidBody;
        return;
    }
    sm_world->addRigidBody(rigidBody);

    RigidNode rigidNode;
    rigidNode.sceneNode = sceneNode;
    rigidNode.mesh = mesh;
    rigidNode.body = rigidBody;
    rigidNode.shapeOwner = std::move(collisionShapeOwner);
    rigidNode.triangleMeshOwner = std::move(triangleMeshOwner);
    sm_rigidNodes.push_back(std::move(rigidNode));
    resetRigidNodeTransformSnapshotNoLock(sceneNode, rigidBody->getWorldTransform());
    PhysicsEngine::registerSceneNodePhysicsTransformCallback(sceneNode);
    ENDLAMBDA
}

void PhysicsEngine::addGhostObject(Identifier sceneNode, std::string settingsFile, Model* mesh) {
    WhaleMLFile db; // WhaleML mode
    if (!settingsFile.empty()) db.load(settingsFile);
    addGhostObject(sceneNode, db, mesh);
}

void PhysicsEngine::addGhostObject(Identifier sceneNode, WhaleMLFile& settings, Model* mesh) {
    BEGINLAMBDA
    WhaleMLFile settingsMutable = settings;
    std::string shapeType = settings.string("collision_shape", "box");

    CollisionShapePtr collisionShapeOwner;
    TriangleMeshPtr triangleMeshOwner;
    vector3df pos;
    quaternion rot;
    vector3df scale;
    auto sceneNodes = SceneManager::sm_sceneNodes.get();
    auto it = sceneNodes->find(sceneNode);
    if (it == sceneNodes->end() || !it->second || it->second->isDestroyPending()) {
        Logger::log(LET_WARN, "Orka, Physics Engine", "Cannot add ghost object: SceneNode " + std::to_string(sceneNode.getValue()) + " no longer exists.");
        return;
    }
    pos = it->second->getPosition();
    rot = it->second->getRotation();
    scale = it->second->getScale();
    sceneNodes.unlock();

    if (shapeType == "box") {
        float boxX = 1.0f, boxY = 1.0f, boxZ = 1.0f;
        if (!readVector3Setting(settingsMutable, "box_size", boxX, boxY, boxZ)) {
            if (!mesh) {
                Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot calculate box size automatically: SceneNode has no mesh.");
                return;
            }
            AABB aabb = mesh->getAABB();
            vector3df boxSize = aabb.getSize() * scale;
            boxX = boxSize.x;
            boxY = boxSize.y;
            boxZ = boxSize.z;
        }
        const std::string shapeKey = "ghost|box|" + formatFloatKey(boxX) + "|" + formatFloatKey(boxY) + "|" + formatFloatKey(boxZ);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return CollisionShapePtr(new btBoxShape(btVector3(boxX / 2.0f, boxY / 2.0f, boxZ / 2.0f)), [](btCollisionShape* s) { delete s; });
        });
    } else if (shapeType == "sphere") {
        float radius = 0.5f;
        if (settings.hasKey("sphere_radius")) {
            radius = settings["sphere_radius"].asFloat();
        } else {
            if (!mesh) {
                Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot calculate sphere radius automatically: SceneNode has no mesh.");
                return;
            }
            radius = mesh->getBS().getRadius() * scale.length();
        }
        const std::string shapeKey = "ghost|sphere|" + formatFloatKey(radius);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return CollisionShapePtr(new btSphereShape(radius), [](btCollisionShape* s) { delete s; });
        });
    } else if (shapeType == "capsule") {
        float radius = settings.floating("capsule_radius", 0.5f);
        float height = settings.floating("capsule_height", 1.0f);
        const std::string shapeKey = "ghost|capsule|" + formatFloatKey(radius) + "|" + formatFloatKey(height);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return CollisionShapePtr(new btCapsuleShape(radius, height), [](btCollisionShape* s) { delete s; });
        });
    } else if (shapeType == "convexhull") {
        if (!mesh) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "ConvexHull shape requested, but this SceneNode has no mesh! WTF??");
            return;
        }
        const std::string shapeKey = "ghost|convexhull|mesh=" + formatPointerKey(mesh) + "|scale=" + formatVector3Key(scale);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return createConvexHullShape(mesh, scale);
        });
    } else if (isConvexDecompositionShape(shapeType)) {
        if (!mesh) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "ConvexDecomposition shape requested, but this SceneNode has no mesh.");
            return;
        }
        const std::string shapeKey = "ghost|convexdecomposition|mesh=" + formatPointerKey(mesh) + "|scale=" + formatVector3Key(scale);
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return createConvexDecompositionShape(mesh, scale);
        });
    } else if (shapeType == "mesh" || shapeType == "staticmesh" || shapeType == "trimesh") {
        if (!mesh) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "Mesh shape obviously not available for this SceneNode without mesh!");
            return;
        }
        const std::string triangleMeshKey = "trimesh|mesh=" + formatPointerKey(mesh) + "|scale=" + formatVector3Key(scale);
        triangleMeshOwner = getOrCreateTriangleMesh(triangleMeshKey, [&]() {
            return createTriangleMesh(mesh, scale);
        });
        if (!triangleMeshOwner) {
            Logger::log(LET_ERROR, "Orka, Physics Engine", "Failed to create triangle mesh collision data.");
            return;
        }

        const std::string shapeKey = "ghost|bvh|" + triangleMeshKey;
        collisionShapeOwner = getOrCreateCollisionShape(shapeKey, [&]() {
            return CollisionShapePtr(new btBvhTriangleMeshShape(triangleMeshOwner.get(), true, true), [](btCollisionShape* s) { delete s; });
        });
    } else {
        collisionShapeOwner = getOrCreateCollisionShape("ghost|fallback_box", [&]() {
            return CollisionShapePtr(new btBoxShape(btVector3(0.5f, 0.5f, 0.5f)), [](btCollisionShape* s) { delete s; });
        });
    }

    btCollisionShape* collisionShape = collisionShapeOwner.get();
    if (!collisionShape) {
        Logger::log(LET_ERROR, "Orka, Physics Engine", "Failed to create collision shape for ghost object.");
        return;
    }
    btGhostObject* ghostObject = new btGhostObject();
    ghostObject->setCollisionShape(collisionShape);
    ghostObject->setCollisionFlags(ghostObject->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(pos.x, pos.y, pos.z));
    startTransform.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));
    ghostObject->setWorldTransform(startTransform);

    if (sm_world == nullptr) {
        Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot add ghost object: physics world not initialized yet.");
        delete ghostObject;
        return;
    }
    sm_world->addCollisionObject(ghostObject, btBroadphaseProxy::SensorTrigger, btBroadphaseProxy::AllFilter);

    GhostObject g;
    g.body = ghostObject;
    g.mesh = mesh;
    g.sceneNode = sceneNode;
    g.shapeOwner = std::move(collisionShapeOwner);
   g.triangleMeshOwner = std::move(triangleMeshOwner);
    sm_ghostObjects.push_back(std::move(g));
    PhysicsEngine::registerSceneNodePhysicsTransformCallback(sceneNode);
    ENDLAMBDA
}

void PhysicsEngine::addSoftBody(Identifier sceneNode, std::string settingsFile, Model* mesh, vector3df scale) {
    WhaleMLFile db; // WhaleML mode
    if (!settingsFile.empty()) db.load(settingsFile);
    addSoftBody(sceneNode, db, mesh, scale);
}

void PhysicsEngine::addSoftBody(Identifier sceneNode, WhaleMLFile& settings, Model* mesh, vector3df scale) {
    BEGINLAMBDA
    if (!mesh) {
        Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot create SoftBody: SceneNode has no mesh assigned!");
        return;
    }

    vector3df pos(0.0f);
    auto sceneNodes = SceneManager::sm_sceneNodes.get();
    auto it = sceneNodes->find(sceneNode);
    if (it == sceneNodes->end() || !it->second || it->second->isDestroyPending()) {
        Logger::log(LET_WARN, "Orka, Physics Engine", "Cannot add soft body: SceneNode " + std::to_string(sceneNode.getValue()) + " no longer exists.");
        return;
    }
    pos = it->second->getPosition();
    sceneNodes.unlock();

    std::vector<float> vertexPositions;
    std::vector<int> triangleIndices;
    int vertexOffset = 0;
    for (Mesh* m : mesh->getMeshes()) {
        if (!m) continue;
        auto& vertices = m->getVertexBuffer();
        auto& indices  = m->getIndexBuffer();
        size_t vertexCount = vertices.size();
        size_t indexCount  = indices.size();

        // Vertices
        for (size_t i = 0; i < vertexCount; ++i) {
            const vector3df& p = vertices[i].position;
            vertexPositions.push_back(p.x * scale.x);
            vertexPositions.push_back(p.y * scale.y);
            vertexPositions.push_back(p.z * scale.z);
        }
        // Indices
        for (size_t i = 0; i < indexCount; ++i) {
            triangleIndices.push_back(static_cast<int>(indices[i]) + vertexOffset);
        }
        vertexOffset += static_cast<int>(vertexCount);
    }
    size_t indexCount = triangleIndices.size();

    if (vertexPositions.empty() || indexCount < 3) {
        Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot create SoftBody: invalid mesh data (no vertices or not enough indices)");
        return;
    }

    btSoftBody* softBody = btSoftBodyHelpers::CreateFromTriMesh(
        *sm_softBodyWorldInfo,
        vertexPositions.data(),
        triangleIndices.data(),
        static_cast<int>(indexCount / 3));
    const btVector3 offset(pos.x, pos.y, pos.z);
    for (int i = 0; i < softBody->m_nodes.size(); ++i) {
        softBody->m_nodes[i].m_x += offset;
        softBody->m_nodes[i].m_q += offset;
    }

    float totalMass = settings.floating("mass", 1.0f);
    float linearStiffness = settings.floating("linear_stiffness", 0.5f);
    float angularStiffness = settings.floating("angular_stiffness", 1.0f);
    float volumeStiffness = settings.floating("volume_stiffness", 1.0f);
    float dampingCoeff = settings.floating("damping", 0.01f);
    float frictionCoeff = settings.floating("friction", 0.5f);
    float poseMatchingCoeff = settings.floating("pose_matching", 0.0f);
    float pressureCoeff = settings.floating("pressure", 0.0f);
    int bendingConstraints = settings.integer("bending_constraints", 2);
    //int collisionFlags = settings.integer("collision_flags", btSoftBody::fCollision::SDF_RS | btSoftBody::fCollision::CL_RS);

    softBody->m_materials[0]->m_kLST = linearStiffness;
    softBody->m_materials[0]->m_kAST = angularStiffness;
    softBody->m_materials[0]->m_kVST = volumeStiffness;
    softBody->m_cfg.kDP = dampingCoeff;
    softBody->m_cfg.kDF = frictionCoeff;
    softBody->m_cfg.kMT = poseMatchingCoeff;
    softBody->m_cfg.kPR = pressureCoeff;
    softBody->m_cfg.collisions = btSoftBody::fCollision::SDF_RD | btSoftBody::fCollision::CL_RS | btSoftBody::fCollision::CL_SS;

    softBody->generateBendingConstraints(bendingConstraints);
    softBody->setTotalMass(totalMass);

    if (sm_world == nullptr) {
        Logger::log(LET_ERROR, "Orka, Physics Engine", "Cannot add soft body: physics world not initialized yet.");
        delete softBody;
        return;
    }
    sm_world->addSoftBody(softBody);

    SoftNode softNode;
    softNode.sceneNode = sceneNode;
    softNode.mesh = mesh;
    softNode.body = softBody;
    sm_softNodes.push_back(softNode);
    PhysicsEngine::registerSceneNodePhysicsTransformCallback(sceneNode);
    ENDLAMBDA
}

void PhysicsEngine::removeBody(Identifier sceneNode) {
    removeBodies(std::vector<Identifier>{ sceneNode });
}

void PhysicsEngine::removeBodies(const std::vector<Identifier>& sceneNodes) {
    if (sceneNodes.empty()) return;

    scheduleBlocking([sceneNodes]() {
        for (Identifier sceneNode : sceneNodes) {
            deleteBodyNoLock(sceneNode);
        }
    });
}

bool PhysicsEngine::deleteBodyNoLock(Identifier sceneNode) {
    PhysicsEngine::clearSceneNodePhysicsTransformCallback(sceneNode);
    sm_moveTasks.erase(sceneNode);

    for (auto it = sm_rigidNodes.begin(); it != sm_rigidNodes.end(); ++it) {
        if (it->sceneNode != sceneNode) continue;
        if (it->body) {
            if (sm_world && it->active) sm_world->removeRigidBody(it->body);
            delete it->body->getMotionState();
            delete it->body;
        }
        it->shapeOwner.reset();
        it->triangleMeshOwner.reset();
        sm_rigidNodes.erase(it);
        return true;
    }

    for (auto it = sm_softNodes.begin(); it != sm_softNodes.end(); ++it) {
        if (it->sceneNode != sceneNode) continue;
        if (it->body) {
            if (sm_world && it->active) sm_world->removeSoftBody(it->body);
            delete it->body;
        }
        sm_softNodes.erase(it);
        return true;
    }

    for (auto it = sm_ghostObjects.begin(); it != sm_ghostObjects.end(); ++it) {
        if (it->sceneNode != sceneNode) continue;
        if (it->body) {
            if (sm_world && it->active) sm_world->removeCollisionObject(it->body);
            delete it->body;
        }
        it->shapeOwner.reset();
        it->triangleMeshOwner.reset();
        sm_ghostObjects.erase(it);
        return true;
    }

    return false;
}

// Updates a softbody
void updateSoftBodyMesh(Model* mesh, btSoftBody* softBody) {
    if (!mesh || !softBody) return;

    size_t vertexOffset = 0;
    for (Mesh* m : mesh->getMeshes()) {
        if (!m) continue;

        auto& vbuf = m->getVertexBuffer();
        auto& ibuf = m->getIndexBuffer();

        const size_t vertexCount = vbuf.size();
        if (vertexOffset + vertexCount > softBody->m_nodes.size()) break;

        std::vector<vector3df> normals(vertexCount, vector3df(0.0f));

        for (size_t i = 0; i + 2 < ibuf.size(); i += 3) {
            const size_t i0 = vertexOffset + static_cast<size_t>(ibuf[i]);
            const size_t i1 = vertexOffset + static_cast<size_t>(ibuf[i + 1]);
            const size_t i2 = vertexOffset + static_cast<size_t>(ibuf[i + 2]);
            if (i0 >= softBody->m_nodes.size() || i1 >= softBody->m_nodes.size() || i2 >= softBody->m_nodes.size()) continue;

            const btVector3& p0 = softBody->m_nodes[i0].m_x;
            const btVector3& p1 = softBody->m_nodes[i1].m_x;
            const btVector3& p2 = softBody->m_nodes[i2].m_x;

            vector3df v0(p0.x(), p0.y(), p0.z());
            vector3df v1(p1.x(), p1.y(), p1.z());
            vector3df v2(p2.x(), p2.y(), p2.z());

            vector3df faceNormal = glm::cross(v1 - v0, v2 - v0);

            normals[i0] += faceNormal;
            normals[i1] += faceNormal;
            normals[i2] += faceNormal;
        }

        for (auto& n : normals) {
            float len = glm::length(n);
            if (len > 1e-6f)
                n /= len;
            else
                n = vector3df(0.0f, 1.0f, 0.0f);
        }

        for (size_t i = 0; i < vertexCount; ++i) {
            const btVector3& p = softBody->m_nodes[vertexOffset + i].m_x;
            vbuf[i].position = vector3df(p.x(), p.y(), p.z());
            vbuf[i].normal   = normals[i];
        }

        vbuf.markDirty();
        ibuf.markDirty();
        vertexOffset += vertexCount;
    }
}

void PhysicsEngine::update(bool blockThread) {
    auto start = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(sm_physicsMutex);
    bool block = blockThread && sm_physicsThreadRunning;
    const float fixedStep = sm_simulationSpeed > 0 ? 1.0f / static_cast<float>(sm_simulationSpeed) : (1.0f / 60.0f);

    auto tasksInternal = sm_physicsThreadTasks.get();
    auto actualTasks = tasksInternal.get();
    for (auto& task : *actualTasks) {
        task();
    }
    actualTasks->clear();
    tasksInternal.unlock();

    if (sm_pauseRequests.load(std::memory_order_acquire) > 0) {
        sm_hasLastUpdateTime = false;
        clearTransformFrame();
        lock.unlock();
        if (block) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(fixedStep * 1000.0f)));
        }
        return;
    }

    const auto currentTime = std::chrono::steady_clock::now();
    float dt = fixedStep;
    if (sm_hasLastUpdateTime) {
        dt = std::chrono::duration<float>(currentTime - sm_lastUpdateTime).count();
    }
    sm_lastUpdateTime = currentTime;
    sm_hasLastUpdateTime = true;
    if (dt < 0.0f) dt = fixedStep;
    if (dt > 0.1f) dt = 0.1f; // Clamp long stalls to avoid solver explosions.

    if (sm_world != nullptr) {
        const int maxSubSteps = sm_simulationSpeed > 0 ? 10 : 4;
        sm_world->stepSimulation(dt, maxSubSteps, fixedStep);
    }

    TransformFrame nextTransformFrame;
    nextTransformFrame.rigidBodies.reserve(sm_rigidNodes.size());
    nextTransformFrame.simulationTime = currentTime;
    nextTransformFrame.fixedStep = fixedStep;
    nextTransformFrame.valid = true;

    for (auto& rb : sm_rigidNodes) {
        if (!rb.active) continue;

        const btTransform transform = rb.body->getWorldTransform();
        const btVector3 position = transform.getOrigin();
        const btQuaternion rotation = transform.getRotation();
        const vector3df newPosition(position.x(), position.y(), position.z());
        const quaternion newRotation(rotation.w(), rotation.x(), rotation.y(), rotation.z());
        if (!rb.hasTransformSnapshot) {
            rb.previousPosition = newPosition;
            rb.previousRotation = newRotation;
            rb.hasTransformSnapshot = true;
        } else {
            rb.previousPosition = rb.currentPosition;
            rb.previousRotation = rb.currentRotation;
        }
        rb.currentPosition = newPosition;
        rb.currentRotation = newRotation;
        nextTransformFrame.rigidBodies.push_back({
            rb.sceneNode,
            rb.previousPosition,
            rb.currentPosition,
            rb.previousRotation,
            rb.currentRotation
        });
    }
    {
        std::lock_guard<std::mutex> transformLock(sm_transformFrameMutex);
        sm_transformFrame = std::move(nextTransformFrame);
    }
    for (const auto& sb : sm_softNodes) {
        if (!sb.active) continue;
        updateSoftBodyMesh(sb.mesh, sb.body);
    }

    for (const auto& cb : sm_collisionCallbacks) {
        auto& cc = cb.second;
        if (cc.nodeB != -1) {
            if (PhysicsEngine::areCollidingNoLock(cc.nodeA, cc.nodeB)) cc.callback();
        } else {
            for (const auto& n : sm_rigidNodes) {
                if (!n.active) continue;
                if (n.sceneNode != cc.nodeA && PhysicsEngine::areCollidingNoLock(cc.nodeA, n.sceneNode)) cc.altCallback(n.sceneNode);
            }
            for (const auto& n : sm_softNodes) {
                if (!n.active) continue;
                if (n.sceneNode != cc.nodeA && PhysicsEngine::areCollidingNoLock(cc.nodeA, n.sceneNode)) cc.altCallback(n.sceneNode);
            }
            for (const auto& n : sm_ghostObjects) {
                if (!n.active) continue;
                if (n.sceneNode != cc.nodeA && PhysicsEngine::areCollidingNoLock(cc.nodeA, n.sceneNode)) cc.altCallback(n.sceneNode);
            }
        }
    }

    auto moveSceneNodes = SceneManager::sm_sceneNodes.get();
    for (auto it = sm_moveTasks.begin(); it != sm_moveTasks.end(); ) {
        auto& task = it->second;
        task.elapsedTime += dt;
        auto* obj = getBodyNoLock(task.sceneNode);
        if (!obj) {
            it = sm_moveTasks.erase(it);
            continue;
        }
        auto* body = btRigidBody::upcast(obj);
        if (!body) {
            it = sm_moveTasks.erase(it);
            continue;
        }

        auto sceneNodeIt = moveSceneNodes->find(task.sceneNode);
        if (sceneNodeIt == moveSceneNodes->end() || !sceneNodeIt->second) {
            it = sm_moveTasks.erase(it);
            continue;
        }

        vector3df currentPos = sceneNodeIt->second->getPosition();
        vector3df toTarget = task.targetPos - currentPos;
        float timeLeft = task.totalTime - task.elapsedTime;
        if (timeLeft <= 0.01f || toTarget.length() < 0.01f) {
            body->setLinearVelocity(btVector3(0, 0, 0));
            it = sm_moveTasks.erase(it);
            continue;
        }
        vector3df velocity = toTarget / timeLeft;
        body->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
        ++it;
    }
    moveSceneNodes.unlock();

    lock.unlock();

    if (block) {
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        int targetMs = static_cast<int>(fixedStep * 1000.0f);
        int remainingMs = targetMs - static_cast<int>(elapsed.count());
        if (remainingMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(remainingMs));
        }
    }
}

void PhysicsEngine::interpolateTransforms() {
    TransformFrame transformFrame;
    {
        std::lock_guard<std::mutex> lock(sm_transformFrameMutex);
        if (!sm_transformFrame.valid) return;
        transformFrame = sm_transformFrame;
    }

    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - transformFrame.simulationTime).count();
    const float alpha = std::clamp(elapsed / transformFrame.fixedStep, 0.0f, 1.0f);

    auto sceneNodes = SceneManager::sm_sceneNodes.get();
    for (const auto& transform : transformFrame.rigidBodies) {
        auto it = sceneNodes->find(transform.sceneNode);
        if (it == sceneNodes->end() || !it->second) continue;

        const vector3df position = glm::mix(transform.previousPosition, transform.currentPosition, alpha);
        const quaternion rotation = glm::normalize(glm::slerp(transform.previousRotation, transform.currentRotation, alpha));
        it->second->setPosition(position, false);
        it->second->setRotation(rotation, false);
    }
}

void PhysicsEngine::createPhysicsWorld(std::string filepath) {
    BEGINLAMBDA
    WhaleMLFile db; // WhaleML mode
    db.load(filepath);

    float gravityY = db.floating("gravity", -9.81f);
    int solverIterations = db.integer("solverIterations", 14);
    bool splitImpulse = db.boolean("splitImpulse", true);
    float airDensity = db.floating("air_density", 1.2f);
    float waterDensity = db.floating("water_density", 0.0f);
    float waterOffset = db.floating("water_offset", 0.0f);

    btVector3 waterNormal(0, 0, 0);
    float waterNormalX = 0.0f, waterNormalY = 0.0f, waterNormalZ = 0.0f;
    if (readVector3Setting(db, "water_normal", waterNormalX, waterNormalY, waterNormalZ)) {
        waterNormal = btVector3(waterNormalX, waterNormalY, waterNormalZ);
    }
    bool useMultithreaded = false;
    if (db.hasKey("threading.multithreaded")) useMultithreaded = db["threading.multithreaded"].asBool();
    else if (db.hasKey("multithreaded")) useMultithreaded = db["multithreaded"].asBool();

    bool useMtDispatcher = useMultithreaded;
    if (db.hasKey("threading.multithreaded_dispatcher")) useMtDispatcher = db["threading.multithreaded_dispatcher"].asBool();
    else if (db.hasKey("multithreaded_dispatcher")) useMtDispatcher = db["multithreaded_dispatcher"].asBool();

    bool useMtSolver = useMultithreaded;
    if (db.hasKey("threading.multithreaded_solver")) useMtSolver = db["threading.multithreaded_solver"].asBool();
    else if (db.hasKey("multithreaded_solver")) useMtSolver = db["multithreaded_solver"].asBool();

    PhysicsEngine::createPhysicsWorldInternal(
        gravityY,
        gravityY,
        solverIterations,
        splitImpulse,
        airDensity,
        waterDensity,
        waterOffset,
        waterNormal,
        useMtDispatcher,
        useMtSolver
    );
    ENDLAMBDA
}

void PhysicsEngine::createDefaultPhysicsWorld() {
    BEGINLAMBDA
    PhysicsEngine::createPhysicsWorldInternal(
        -9.81f,
        -9.81f * 10.0f,
        14,
        true,
        1.2f,
        0.0f,
        0.0f,
        btVector3(0, 0, 0),
        false,
        false
    );
    ENDLAMBDA
}

void PhysicsEngine::destroyPhysicsWorld() {
    scheduleBlocking([=]() {
    for (auto& rn : sm_rigidNodes) {
        if (rn.body) {
            if (sm_world && rn.active) sm_world->removeRigidBody(rn.body);
            delete rn.body->getMotionState();
            delete rn.body;
        }
        rn.shapeOwner.reset();
        rn.triangleMeshOwner.reset();
    }
    for (auto& sb : sm_softNodes) {
        if (sb.body) {
            if (sm_world && sb.active) sm_world->removeSoftBody(sb.body);
            delete sb.body;
        }
    }
    for (auto& g : sm_ghostObjects) {
        if (g.body) {
            if (sm_world && g.active) sm_world->removeCollisionObject(g.body);
            delete g.body;
        }
        g.shapeOwner.reset();
        g.triangleMeshOwner.reset();
    }
    sm_rigidNodes.clear();
    sm_softNodes.clear();
    sm_ghostObjects.clear();
    sm_moveTasks.clear();
    sm_collisionCallbacks.clear();
    sg_collisionShapeCache.clear();
    sg_triangleMeshCache.clear();
    clearTransformFrame();

    delete sm_world;              sm_world = nullptr;
    delete sm_solver;             sm_solver = nullptr;
    delete sm_broadphase;         sm_broadphase = nullptr;
    delete sm_dispatcher;         sm_dispatcher = nullptr;
    delete sm_collisionConfig;    sm_collisionConfig = nullptr;
    delete sm_softBodyWorldInfo;  sm_softBodyWorldInfo = nullptr;
    delete sm_ghostPairCallback;  sm_ghostPairCallback = nullptr;
    sm_hasLastUpdateTime = false;
    });
}

void PhysicsEngine::init(std::string filepath) {
    sm_hasLastUpdateTime = false;
    clearTransformFrame();
    MiscTools::disableLogging(); // Block uggly white Bullet thread messages
    if (!sm_taskScheduler) {
        sm_taskScheduler = btCreateDefaultTaskScheduler();
    }
    btSetTaskScheduler(sm_taskScheduler);
    MiscTools::enableLogging();
    if (!filepath.empty()) {
        createPhysicsWorld(std::move(filepath));
    } else {
        createDefaultPhysicsWorld();
    }
}

void PhysicsEngine::deinit() {
    stopPhysicsThread();
    destroyPhysicsWorld(); // Destroy the universe BOOOM!
    btSetTaskScheduler(nullptr);
    delete sm_taskScheduler;
    sm_taskScheduler = nullptr;
    sm_hasLastUpdateTime = false;
}

void PhysicsEngine::move(Identifier sceneNode, vector3df newPosition, float time) {
    BEGINLAMBDA
    auto* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    auto* body = btRigidBody::upcast(obj);
    if (!body) return;

    MovementTask task;
    task.sceneNode = sceneNode;
    auto sceneNodes = SceneManager::sm_sceneNodes.get();
    auto it = sceneNodes->find(sceneNode);
    if (it != sceneNodes->end() && it->second) task.startPos = it->second->getPosition();
    else task.startPos = newPosition;
    sceneNodes.unlock();
    task.targetPos = newPosition;
    task.totalTime = time;
    task.elapsedTime = 0.0f;
    sm_moveTasks[sceneNode] = task;
    ENDLAMBDA
}

void PhysicsEngine::applyCentralImpulse(Identifier sceneNode, vector3df impulse) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) { // Only works with RigidBodies I said!
        body->activate(true);
        body->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
    }
    ENDLAMBDA
}

void PhysicsEngine::applyImpulse(Identifier sceneNode, vector3df impulse, vector3df relativePosition) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) {
        body->activate(true);
        body->applyImpulse(
            btVector3(impulse.x, impulse.y, impulse.z),
            btVector3(relativePosition.x, relativePosition.y, relativePosition.z)
        );
    }
    ENDLAMBDA
}

void PhysicsEngine::applyTorqueImpulse(Identifier sceneNode, vector3df torque) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) {
        body->activate(true);
        body->applyTorqueImpulse(btVector3(torque.x, torque.y, torque.z));
    }
    ENDLAMBDA
}

void PhysicsEngine::applyRotationImpulse(Identifier sceneNode, vector3df rotationImpulse) {
    applyTorqueImpulse(sceneNode, rotationImpulse);
}

void PhysicsEngine::applyCentralForce(Identifier sceneNode, vector3df force) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) {
        body->activate(true);
        body->applyCentralForce(btVector3(force.x, force.y, force.z));
    }
    ENDLAMBDA
}

void PhysicsEngine::applyForce(Identifier sceneNode, vector3df force, vector3df relativePosition) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) {
        body->activate(true);
        body->applyForce(
            btVector3(force.x, force.y, force.z),
            btVector3(relativePosition.x, relativePosition.y, relativePosition.z)
        );
    }
    ENDLAMBDA
}

void PhysicsEngine::applyTorque(Identifier sceneNode, vector3df torque) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) {
        body->activate(true);
        body->applyTorque(btVector3(torque.x, torque.y, torque.z));
    }
    ENDLAMBDA
}

void PhysicsEngine::setLinearVelocity(Identifier sceneNode, vector3df velocity) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) {
        body->activate(true);
        body->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    }
    ENDLAMBDA
}

vector3df PhysicsEngine::getLinearVelocity(Identifier sceneNode) {
    vector3df velocity(0.0f);
    scheduleBlocking([sceneNode, &velocity]() {
        btCollisionObject* obj = getBodyNoLock(sceneNode);
        if (!obj) return;
        btRigidBody* body = btRigidBody::upcast(obj);
        if (!body) return;
        const btVector3 bodyVelocity = body->getLinearVelocity();
        velocity = vector3df(bodyVelocity.x(), bodyVelocity.y(), bodyVelocity.z());
    });
    return velocity;
}

void PhysicsEngine::setAngularVelocity(Identifier sceneNode, vector3df velocity) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    btRigidBody* body = btRigidBody::upcast(obj);
    if (body) {
        body->activate(true);
        body->setAngularVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    }
    ENDLAMBDA
}

vector3df PhysicsEngine::getAngularVelocity(Identifier sceneNode) {
    vector3df velocity(0.0f);
    scheduleBlocking([sceneNode, &velocity]() {
        btCollisionObject* obj = getBodyNoLock(sceneNode);
        if (!obj) return;
        btRigidBody* body = btRigidBody::upcast(obj);
        if (!body) return;
        const btVector3 bodyVelocity = body->getAngularVelocity();
        velocity = vector3df(bodyVelocity.x(), bodyVelocity.y(), bodyVelocity.z());
    });
    return velocity;
}

void PhysicsEngine::setPosition(Identifier sceneNode, vector3df position, bool stopMovement) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (obj) {
        if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
            btRigidBody* body = btRigidBody::upcast(obj);
            btTransform trans = body->getWorldTransform();
            trans.setOrigin(btVector3(position.x, position.y, position.z));
            body->setWorldTransform(trans);
            if (body->getMotionState())
                body->getMotionState()->setWorldTransform(trans);
            resetRigidNodeTransformSnapshotNoLock(sceneNode, trans);
            if (!stopMovement) return;
            body->setLinearVelocity(btVector3(0, 0, 0));
            body->setAngularVelocity(btVector3(0, 0, 0));
            auto sceneNodes = SceneManager::sm_sceneNodes.get();
            auto it = sceneNodes->find(sceneNode);
            if (it != sceneNodes->end() && it->second) it->second->setPosition(position, false);
        } else if (obj->getInternalType() == btCollisionObject::CO_SOFT_BODY) { // Since Soft Bodies are hard to move anywhere we have to perform a hack!
            btSoftBody* softBody = btSoftBody::upcast(obj);
            btVector3 center(0, 0, 0);
            int count = softBody->m_nodes.size();
            for (int i = 0; i < count; ++i)
                center += softBody->m_nodes[i].m_x;
            if (count > 0)
                center /= (btScalar)count;
            btVector3 delta = btVector3(position.x, position.y, position.z) - center;

            for (int i = 0; i < softBody->m_nodes.size(); ++i) {
                softBody->m_nodes[i].m_x += delta;
                softBody->m_nodes[i].m_q += delta;
            }
        } else if (obj->getInternalType() == btCollisionObject::CO_GHOST_OBJECT) {
            btGhostObject* ghost = btGhostObject::upcast(obj);
            btTransform trans = ghost->getWorldTransform();
            trans.setOrigin(btVector3(position.x, position.y, position.z));
            ghost->setWorldTransform(trans);
            auto sceneNodes = SceneManager::sm_sceneNodes.get();
            auto it = sceneNodes->find(sceneNode);
            if (it != sceneNodes->end() && it->second) it->second->setPosition(position, false);
        }
    }
    ENDLAMBDA
}

void PhysicsEngine::setRotation(Identifier sceneNode, quaternion rotation, bool stopMovement) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (obj) {
        if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
            btRigidBody* body = btRigidBody::upcast(obj);
            btTransform trans = body->getWorldTransform();
            trans.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
            body->setWorldTransform(trans);
            if (body->getMotionState())
                body->getMotionState()->setWorldTransform(trans);
            resetRigidNodeTransformSnapshotNoLock(sceneNode, trans);
            if (!stopMovement) return;
            body->setLinearVelocity(btVector3(0, 0, 0));
            body->setAngularVelocity(btVector3(0, 0, 0));
            auto sceneNodes = SceneManager::sm_sceneNodes.get();
            auto it = sceneNodes->find(sceneNode);
            if (it != sceneNodes->end() && it->second) it->second->setRotation(rotation, false);
        } else if (obj->getInternalType() == btCollisionObject::CO_GHOST_OBJECT) {
            btGhostObject* ghost = btGhostObject::upcast(obj);
            btTransform trans = ghost->getWorldTransform();
            trans.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
            ghost->setWorldTransform(trans);
            auto sceneNodes = SceneManager::sm_sceneNodes.get();
            auto it = sceneNodes->find(sceneNode);
            if (it != sceneNodes->end() && it->second) it->second->setRotation(rotation, false);
        } else {
            Logger::log(LET_WARN, "Orka, Physics Engine", "Rotation is only supported for rigid bodies and Ghost objects!");
        }
    }
    ENDLAMBDA
}

void PhysicsEngine::setScale(Identifier sceneNode, vector3df scale) {
    BEGINLAMBDA
    btCollisionObject* obj = getBodyNoLock(sceneNode);

    if (!obj) return;

    if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
        RigidNode* rigidNode = nullptr;
        for (auto& node : sm_rigidNodes) {
            if (node.sceneNode == sceneNode) {
                rigidNode = &node;
                break;
            }
        }
        btRigidBody* body = btRigidBody::upcast(obj);
        if (!body) return;
        if (rigidNode && rigidNode->shapeOwner && rigidNode->shapeOwner.use_count() > 1) {
            CollisionShapePtr uniqueShape = cloneCollisionShape(rigidNode->shapeOwner.get(), rigidNode->triangleMeshOwner);
            if (!uniqueShape) {
                Logger::log(LET_WARN, "Orka, Physics Engine", "Cannot detach shared collision shape for scaling SceneNode " + std::to_string(sceneNode.getValue()) + ". Skipping scale update.");
                return;
            }
            body->setCollisionShape(uniqueShape.get());
            rigidNode->shapeOwner = std::move(uniqueShape);
        }

        btCollisionShape* shape = body->getCollisionShape();
        if (!shape) return;
        shape->setLocalScaling(btVector3(scale.x, scale.y, scale.z));
        if (shape->getShapeType() == GIMPACT_SHAPE_PROXYTYPE) {
            btGImpactMeshShape* gimpact = static_cast<btGImpactMeshShape*>(shape);
            gimpact->updateBound();
        }
        btScalar mass = body->getInvMass() != 0.0f ? 1.0f / body->getInvMass() : 0.0f;
        btVector3 inertia(0, 0, 0);
        if (mass > 0.0f)
            shape->calculateLocalInertia(mass, inertia);
        body->setMassProps(mass, inertia);
        body->updateInertiaTensor();
        auto sceneNodes = SceneManager::sm_sceneNodes.get();
        auto it = sceneNodes->find(sceneNode);
        if (it != sceneNodes->end() && it->second) it->second->setScale(scale, false);
    } else if (obj->getInternalType() == btCollisionObject::CO_GHOST_OBJECT) {
        GhostObject* ghostNode = nullptr;
        for (auto& node : sm_ghostObjects) {
            if (node.sceneNode == sceneNode) {
                ghostNode = &node;
                break;
            }
        }
        btGhostObject* ghost = btGhostObject::upcast(obj);
        if (!ghost) return;
        if (ghostNode && ghostNode->shapeOwner && ghostNode->shapeOwner.use_count() > 1) {
            CollisionShapePtr uniqueShape = cloneCollisionShape(ghostNode->shapeOwner.get(), ghostNode->triangleMeshOwner);
            if (!uniqueShape) {
                Logger::log(LET_WARN, "Orka, Physics Engine", "Cannot detach shared ghost collision shape for scaling SceneNode " + std::to_string(sceneNode.getValue()) + ". Skipping scale update.");
                return;
            }
            ghost->setCollisionShape(uniqueShape.get());
            ghostNode->shapeOwner = std::move(uniqueShape);
        }

        btCollisionShape* shape = ghost->getCollisionShape();
        if (!shape) return;
        shape->setLocalScaling(btVector3(scale.x, scale.y, scale.z));
        if (shape->getShapeType() == GIMPACT_SHAPE_PROXYTYPE) {
            btGImpactMeshShape* gimpact = static_cast<btGImpactMeshShape*>(shape);
            gimpact->updateBound();
        }
        auto sceneNodes = SceneManager::sm_sceneNodes.get();
        auto it = sceneNodes->find(sceneNode);
        if (it != sceneNodes->end() && it->second) it->second->setScale(scale, false);
    } else {
        Logger::log(LET_WARN, "Orka, Physics Engine", "Scaling only supported for rigid bodies and ghosts (Yes I'm sorry but scaling soft bodies is like scaling fluids! 🫠 More problems than usecases!");
    }
    ENDLAMBDA
}

void PhysicsEngine::setMass(Identifier sceneNode, float mass) {
    BEGINLAMBDA
    auto obj = getBodyNoLock(sceneNode);
    if (!obj) return;
    if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
        btRigidBody* body = btRigidBody::upcast(obj);
        if (body) {
            btVector3 localInertia(0, 0, 0);
            if (mass != 0.0f)
                body->getCollisionShape()->calculateLocalInertia(mass, localInertia);
            body->setMassProps(mass, localInertia);
            body->updateInertiaTensor();
        }
    } else if (obj->getInternalType() == btCollisionObject::CO_SOFT_BODY) {
        btSoftBody* soft = btSoftBody::upcast(obj);
        if (soft) {
            float massPerNode = mass / soft->m_nodes.size();
            for (int i = 0; i < soft->m_nodes.size(); ++i)
                soft->setMass(i, massPerNode);
        }
    } else {
        Logger::log(LET_WARN, "Orka, Physics Engine", "Oops! Remember: setMass only works with Rigid and Soft bodies!");
    }
    ENDLAMBDA
}

float PhysicsEngine::getMass(Identifier sceneNode) {
    float mass = 0.0f;
    scheduleBlocking([sceneNode, &mass]() {
        btCollisionObject* obj = getBodyNoLock(sceneNode);
        if (!obj) return;
        if (obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getInvMass() != 0.0f)
                mass = 1.0f / body->getInvMass();
        } else if (obj->getInternalType() == btCollisionObject::CO_SOFT_BODY) {
            btSoftBody* body = btSoftBody::upcast(obj);
            if (body)
                mass = body->getTotalMass();
        }
    });
    return mass;
}

btCollisionObject* PhysicsEngine::getBody(Identifier sceneNode) {
    std::unique_lock<std::mutex> lock(sm_physicsMutex, std::defer_lock);
    if (!amIPhysicsThread()) {
        lock.lock();
    }
    btCollisionObject* body = getBodyNoLock(sceneNode);
    if (body) return body;
    Logger::log(LET_WARN, "Orka, Physics Engine", "SceneNode with ID " + std::to_string(sceneNode.getValue()) + " not found or added to the physics world");
    return nullptr;
}

bool PhysicsEngine::hasBody(Identifier sceneNode) {
    std::unique_lock<std::mutex> lock(sm_physicsMutex, std::defer_lock);
    if (!amIPhysicsThread()) {
        lock.lock();
    }
    return hasBodyNoLock(sceneNode);
}

btCollisionObject* PhysicsEngine::getBodyNoLock(Identifier sceneNode) {
    {
        auto sceneNodes = SceneManager::sm_sceneNodes.get();
        auto it = sceneNodes->find(sceneNode);
        if (it == sceneNodes->end() || !it->second || it->second->isDestroyPending()) return nullptr;
    }

    for (auto& n : sm_rigidNodes) if (n.sceneNode == sceneNode && n.active) return n.body;
    for (auto& n : sm_softNodes) if (n.sceneNode == sceneNode && n.active) return n.body;
    for (auto& n : sm_ghostObjects) if (n.sceneNode == sceneNode && n.active) return n.body;
    return nullptr;
}

bool PhysicsEngine::hasBodyNoLock(Identifier sceneNode) {
    return getBodyNoLock(sceneNode) != nullptr;
}

Identifier PhysicsEngine::getSceneNodeNoLock(const btCollisionObject* body) {
    if (!body) return Identifier::INVALID;
    for (const auto& n : sm_rigidNodes) if (n.body == body && n.active) return n.sceneNode;
    for (const auto& n : sm_softNodes) if (n.body == body && n.active) return n.sceneNode;
    for (const auto& n : sm_ghostObjects) if (n.body == body && n.active) return n.sceneNode;
    return Identifier::INVALID;
}

Identifier PhysicsEngine::addCollisionCallback(std::function<void()> callback, Identifier sceneNodeA, Identifier sceneNodeB) {
    Identifier id = sm_collisionCallbackIDCounter++;
    CollisionCallback c;
    c.callback = callback;
    c.nodeA = sceneNodeA;
    c.nodeB = sceneNodeB;
    BEGINLAMBDA
    sm_collisionCallbacks[id] = c;
    ENDLAMBDA
    return id;
}

Identifier PhysicsEngine::addCollisionCallback(std::function<void(Identifier sceneNodeB)> callback, Identifier sceneNodeA) {
    Identifier id = sm_collisionCallbackIDCounter++;
    CollisionCallback c;
    c.altCallback = callback;
    c.nodeA = sceneNodeA;
    c.nodeB = Identifier::INVALID;
    BEGINLAMBDA
    sm_collisionCallbacks[id] = c;
    ENDLAMBDA
    return id;
}

void PhysicsEngine::removeCollisionCallback(Identifier id) {
    BEGINLAMBDA
    sm_collisionCallbacks.erase(id);
    ENDLAMBDA
}

void PhysicsEngine::clearCollisionCallbacks() {
    BEGINLAMBDA
    sm_collisionCallbacks.clear();
    sm_collisionCallbackIDCounter = 0;
    ENDLAMBDA
}

bool PhysicsEngine::internalAreCollidingNoLock(btCollisionObject* objA, btCollisionObject* objB, btDynamicsWorld* world) { // Helper function that checks a collision between 2 collision objects
    if (!objA || !objB || !world) return false;
    if (objA == objB) return false; // Same object can't collide with itself

    int numManifolds = world->getDispatcher()->getNumManifolds();
    for (int i = 0; i < numManifolds; ++i) {
        btPersistentManifold* manifold = world->getDispatcher()->getManifoldByIndexInternal(i);
        const btCollisionObject* body0 = static_cast<const btCollisionObject*>(manifold->getBody0());
        const btCollisionObject* body1 = static_cast<const btCollisionObject*>(manifold->getBody1());
        if ((body0 == objA && body1 == objB) || (body0 == objB && body1 == objA)) {
            if (manifold->getNumContacts() > 0) {
                for (int j = 0; j < manifold->getNumContacts(); ++j) {
                    const btManifoldPoint& pt = manifold->getContactPoint(j);
                    if (pt.getDistance() < 0.1f) { // Must be that big
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool areOverlappingNoLock(btGhostObject* ghost, btCollisionObject* other) { // Helper function that checks if a collision object is overlapping a GhostObject
    if (!ghost || !other) return false;

    int numOverlaps = ghost->getNumOverlappingObjects();
    for (int i = 0; i < numOverlaps; ++i) {
        if (ghost->getOverlappingObject(i) == other) {
            return true;
        }
    }
    return false;
}

static void addCollisionNode(std::vector<Identifier>& collisions, Identifier sceneNode) {
    if (sceneNode == Identifier::INVALID) return;

    if (std::find(collisions.begin(), collisions.end(), sceneNode) == collisions.end()) {
        collisions.push_back(sceneNode);
    }
}

static void addCollisionPair(std::vector<Identifier>& collisions, Identifier sceneNodeA, Identifier sceneNodeB) {
    if (sceneNodeA == sceneNodeB) return;

    addCollisionNode(collisions, sceneNodeA);
    addCollisionNode(collisions, sceneNodeB);
}

static bool manifoldHasContact(const btPersistentManifold* manifold) {
    if (!manifold) return false;

    for (int i = 0; i < manifold->getNumContacts(); ++i) {
        const btManifoldPoint& pt = manifold->getContactPoint(i);
        if (pt.getDistance() < 0.1f) {
            return true;
        }
    }
    return false;
}

bool PhysicsEngine::areCollidingNoLock(Identifier a, Identifier b) {
    auto objA = getBodyNoLock(a);
    auto objB = getBodyNoLock(b);
    if (!objA || !objB) return false;

    if (objA->getInternalType() == btCollisionObject::CO_GHOST_OBJECT || objB->getInternalType() == btCollisionObject::CO_GHOST_OBJECT) { // Some mastermind tried to check a collision with a ghost!
        if (objA->getInternalType() == btCollisionObject::CO_GHOST_OBJECT) {
            return areOverlappingNoLock(btGhostObject::upcast(objA), objB);
        } else { // ObjB is a ghost
            return areOverlappingNoLock(btGhostObject::upcast(objB), objA);
        }
    }
    return internalAreCollidingNoLock(objA, objB, sm_world); // Use the helper function
}

bool PhysicsEngine::areColliding(Identifier a, Identifier b) {
    std::unique_lock<std::mutex> lock(sm_physicsMutex, std::defer_lock);
    if (!amIPhysicsThread()) {
        lock.lock();
    }
    return areCollidingNoLock(a, b);
}

std::vector<Identifier> PhysicsEngine::collisionQuery(Identifier sceneNode) {
    std::unique_lock<std::mutex> lock(sm_physicsMutex, std::defer_lock);
    if (!amIPhysicsThread()) {
        lock.lock();
    }

    std::vector<Identifier> results;
    btCollisionObject* obj = getBodyNoLock(sceneNode);
    if (!obj) return results;

    // Ouch! That much recursive iterations! Don't think so? Read areColliding!
    for (const auto& n : sm_rigidNodes) {
        if (!n.active) continue;
        if (n.sceneNode != sceneNode && areCollidingNoLock(sceneNode, n.sceneNode))
            results.push_back(n.sceneNode);
    }
    for (const auto& n : sm_softNodes) {
        if (!n.active) continue;
        if (n.sceneNode != sceneNode && areCollidingNoLock(sceneNode, n.sceneNode))
            results.push_back(n.sceneNode);
    }
    for (const auto& n : sm_ghostObjects) {
        if (!n.active) continue;
        if (n.sceneNode != sceneNode && areCollidingNoLock(sceneNode, n.sceneNode))
            results.push_back(n.sceneNode);
    }
    return results;
}

std::vector<Identifier> PhysicsEngine::getCollidingSceneNodes() {
    std::unique_lock<std::mutex> lock(sm_physicsMutex, std::defer_lock);
    if (!amIPhysicsThread()) {
        lock.lock();
    }

    std::vector<Identifier> results;
    if (!sm_world || !sm_world->getDispatcher()) return results;

    const int numManifolds = sm_world->getDispatcher()->getNumManifolds();
    for (int i = 0; i < numManifolds; ++i) {
        btPersistentManifold* manifold = sm_world->getDispatcher()->getManifoldByIndexInternal(i);
        if (!manifoldHasContact(manifold)) continue;

        const btCollisionObject* body0 = static_cast<const btCollisionObject*>(manifold->getBody0());
        const btCollisionObject* body1 = static_cast<const btCollisionObject*>(manifold->getBody1());
        addCollisionPair(results, getSceneNodeNoLock(body0), getSceneNodeNoLock(body1));
    }

    for (const auto& n : sm_ghostObjects) {
        if (!n.active || !n.body) continue;

        const int numOverlaps = n.body->getNumOverlappingObjects();
        for (int i = 0; i < numOverlaps; ++i) {
            addCollisionPair(results, n.sceneNode, getSceneNodeNoLock(n.body->getOverlappingObject(i)));
        }
    }

    return results;
}

RaycastResult PhysicsEngine::raycast(vector3df from, vector3df to) {
    std::unique_lock<std::mutex> lock(sm_physicsMutex, std::defer_lock);
    if (!amIPhysicsThread()) {
        lock.lock();
    }

    RaycastResult result;
    if (!sm_world) return result;

    btVector3 rayFrom(from.x, from.y, from.z);
    btVector3 rayTo(to.x, to.y, to.z);
    btCollisionWorld::ClosestRayResultCallback callback(rayFrom, rayTo);
    sm_world->rayTest(rayFrom, rayTo, callback);

    if (!callback.hasHit()) return result;

    result.hit = true;
    result.sceneNode = getSceneNodeNoLock(callback.m_collisionObject);
    result.hitPoint = vector3df(
        callback.m_hitPointWorld.x(),
        callback.m_hitPointWorld.y(),
        callback.m_hitPointWorld.z()
    );
    result.hitNormal = vector3df(
        callback.m_hitNormalWorld.x(),
        callback.m_hitNormalWorld.y(),
        callback.m_hitNormalWorld.z()
    );
    result.hitFraction = callback.m_closestHitFraction;
    return result;
}

std::vector<RaycastResult> PhysicsEngine::raycastAll(vector3df from, vector3df to) {
    std::unique_lock<std::mutex> lock(sm_physicsMutex, std::defer_lock);
    if (!amIPhysicsThread()) {
        lock.lock();
    }

    std::vector<RaycastResult> results;
    if (!sm_world) return results;

    btVector3 rayFrom(from.x, from.y, from.z);
    btVector3 rayTo(to.x, to.y, to.z);
    btCollisionWorld::AllHitsRayResultCallback callback(rayFrom, rayTo);
    sm_world->rayTest(rayFrom, rayTo, callback);

    if (!callback.hasHit()) return results;

    const int hitCount = callback.m_collisionObjects.size();
    results.reserve(hitCount);
    for (int i = 0; i < hitCount; ++i) {
        RaycastResult result;
        result.hit = true;
        result.sceneNode = getSceneNodeNoLock(callback.m_collisionObjects[i]);
        result.hitPoint = vector3df(
            callback.m_hitPointWorld[i].x(),
            callback.m_hitPointWorld[i].y(),
            callback.m_hitPointWorld[i].z()
        );
        result.hitNormal = vector3df(
            callback.m_hitNormalWorld[i].x(),
            callback.m_hitNormalWorld[i].y(),
            callback.m_hitNormalWorld[i].z()
        );
        result.hitFraction = callback.m_hitFractions[i];
        results.push_back(result);
    }

    std::sort(results.begin(), results.end(), [](const RaycastResult& a, const RaycastResult& b) {
        return a.hitFraction < b.hitFraction;
    });
    return results;
}

void PhysicsEngine::pauseSimulation() {
    const int previous = sm_pauseRequests.fetch_add(1, std::memory_order_acq_rel);
    if (previous == 0) {
        std::lock_guard<std::mutex> lock(sm_physicsMutex);
        sm_hasLastUpdateTime = false;
    }
}

void PhysicsEngine::resumeSimulation() {
    const int previous = sm_pauseRequests.fetch_sub(1, std::memory_order_acq_rel);
    if (previous <= 1) {
        std::lock_guard<std::mutex> lock(sm_physicsMutex);
        sm_pauseRequests.store(0, std::memory_order_release);
        sm_hasLastUpdateTime = false;
    }
}

void PhysicsEngine::startPhysicsThread() {
    if (sm_physicsThreadRunning.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    sm_hasLastUpdateTime = false;

    std::promise<void> threadReady;
    std::future<void> readyFuture = threadReady.get_future();
    sm_physicsThread = std::thread([ready = std::move(threadReady)]() mutable {
        {
            auto physicsThreadID = sm_physicsThreadID.get();
            *physicsThreadID = std::this_thread::get_id();
            physicsThreadID.unlock();
        }
        ready.set_value();
        while (sm_physicsThreadRunning.load(std::memory_order_acquire)) {
            PhysicsEngine::update();
        }
        {
            auto physicsThreadID = sm_physicsThreadID.get();
            *physicsThreadID = std::thread::id{};
            physicsThreadID.unlock();
        }
    });
    readyFuture.wait();
}

void PhysicsEngine::stopPhysicsThread() {
    sm_physicsThreadRunning.store(false, std::memory_order_release);
    if (sm_physicsThread.joinable()) {
        sm_physicsThread.join();
    } else {
        auto physicsThreadID = sm_physicsThreadID.get();
        *physicsThreadID = std::thread::id{};
        physicsThreadID.unlock();
    }
    sm_hasLastUpdateTime = false;
    clearTransformFrame();
}

bool PhysicsEngine::amIPhysicsThread() {
    if (!sm_physicsThreadRunning.load(std::memory_order_acquire)) {
        return false;
    }
    auto physicsThreadID = sm_physicsThreadID.get();
    bool isPhysicsThread = *physicsThreadID == std::this_thread::get_id();
    physicsThreadID.unlock();
    return isPhysicsThread;
}

#endif
