/*
 * File: CameraSceneNode.cpp
 *
 * CameraSceneNode Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "CameraSceneNode.hpp"
#include "SceneManager.hpp"

CameraSceneNode& CameraSceneNode::create(bool createNew, bool setAsActiveCamera) {
    if (SceneManager::hasCamera() && !createNew) {
        return SceneManager::getCamera();
    }

    CameraSceneNode& camera = SceneNode::create<CameraSceneNode>(false);
    camera.setPosition(vector3df(0.0f, 0.0f, -3.0f));
    camera.setRotation(quaternion(1.0f, 0.0f, 0.0f, 0.0f));
    camera.setFoV(60.0f);
    camera.setNearPlane(0.05f);
    camera.setFarPlane(200.0f);
    if (setAsActiveCamera) SceneManager::setCamera(&camera);
    return camera;
}

CameraSceneNode::~CameraSceneNode() {
    SceneManager::removeCamera(this);
}

void CameraSceneNode::destroy() {
    if (isDestroyPending()) return;
    SceneNode::destroy();
    SceneManager::removeCamera(this);
    if (!m_addedToSceneManager) {
        queueScriptOperation([this] { SceneManager::queueCameraDestroy(this); });
    }
}

vector3df CameraSceneNode::getLookDirection() const {
    quaternion rotation = getRotation();
    const float rotationLength = glm::length(rotation);
    if (rotationLength > 0.000001f) {
        rotation = glm::normalize(rotation);
    } else {
        rotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }

    vector3df lookDirection = glm::mat3_cast(rotation) * vector3df(0.0f, 0.0f, 1.0f);
    const float lookLength = glm::length(lookDirection);
    if (lookLength > 0.000001f) {
        lookDirection /= lookLength;
    } else {
        lookDirection = vector3df(0.0f, 0.0f, 1.0f);
    }
    return lookDirection;
}

