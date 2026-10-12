/*
 * File: OrbitCameraSceneNode.cpp
 *
 * OrbitCameraSceneNode Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "OrbitCameraSceneNode.hpp"
#include "SceneManager.hpp"

#include <algorithm>
#include <cmath>

OrbitCameraSceneNode& OrbitCameraSceneNode::create(bool createNew, bool setAsActiveCamera) {
    if (SceneManager::hasCamera() && !createNew) {
        try {
            return SceneManager::getCamera().as<OrbitCameraSceneNode>();
        }
        catch (const std::bad_cast&) {}
    }

    OrbitCameraSceneNode& camera = SceneNode::create<OrbitCameraSceneNode>(false);
    camera.setPosition(vector3df(0.0f, 0.0f, -3.0f));
    camera.setRotation(quaternion(1.0f, 0.0f, 0.0f, 0.0f));
    camera.setFoV(60.0f);
    camera.setNearPlane(0.05f);
    camera.setFarPlane(200.0f);
    if (setAsActiveCamera) SceneManager::setCamera(&camera);
    return camera;
}

void OrbitCameraSceneNode::update(float delta) {
    updateScript(delta);
    if (!m_orbitEnabled) return;

    m_orbitAngle = radians(static_cast<float>(m_orbitAngle) + delta * static_cast<float>(m_orbitSpeed));
    applyOrbitTransform();
}

void OrbitCameraSceneNode::setOrbitPoint(const vector3df& point) {
    m_orbitTargetPoint = point;
    m_orbitTargetNodeID = -1;
    m_orbitEnabled = true;
    applyOrbitTransform();
}

void OrbitCameraSceneNode::setOrbitObject(Identifier objectID) {
    m_orbitTargetNodeID = objectID;
    if (SceneManager::sceneNodeExists(objectID)) {
        m_orbitTargetPoint = SceneManager::getSceneNode(objectID).getPosition();
    }
    m_orbitEnabled = true;
    applyOrbitTransform();
}

void OrbitCameraSceneNode::setOrbitDirection(CameraOrbitDirection direction) {
    m_orbitDirection = direction;
    if (m_orbitEnabled) applyOrbitTransform();
}

void OrbitCameraSceneNode::setOrbitDistance(float distance) {
    m_orbitDistance = distance;
    if (m_orbitEnabled) applyOrbitTransform();
}

void OrbitCameraSceneNode::setOrbitHeight(float height) {
    m_orbitHeight = height;
    if (m_orbitEnabled) applyOrbitTransform();
}

void OrbitCameraSceneNode::setOrbitBreadth(float breadth) {
    m_orbitBreadth = breadth;
    if (m_orbitEnabled) applyOrbitTransform();
}

void OrbitCameraSceneNode::setOrbitAngle(radians angle) {
    m_orbitAngle = angle;
    if (m_orbitEnabled) applyOrbitTransform();
}

vector3df OrbitCameraSceneNode::getOrbitTargetPosition() const {
    if (m_orbitTargetNodeID == -1) return m_orbitTargetPoint;

    if (!SceneManager::sceneNodeExists(m_orbitTargetNodeID)) return m_orbitTargetPoint;
    const SceneNode* targetNode = &SceneManager::getSceneNode(m_orbitTargetNodeID);
    return targetNode->getPosition();
}

void OrbitCameraSceneNode::applyOrbitTransform() {
    const vector3df targetPosition = getOrbitTargetPosition();
    const float safeDistance = std::max(m_orbitDistance, 0.0001f);
    vector3df offset(0.0f, 0.0f, 0.0f);

    if (m_orbitDirection == COD_VERTICAL) {
        offset = vector3df(m_orbitBreadth, std::sin(static_cast<float>(m_orbitAngle)) * safeDistance, std::cos(static_cast<float>(m_orbitAngle)) * safeDistance);
    } else {
        offset = vector3df(std::cos(static_cast<float>(m_orbitAngle)) * safeDistance, m_orbitHeight, std::sin(static_cast<float>(m_orbitAngle)) * safeDistance);
    }

    const vector3df cameraPosition = targetPosition + offset;
    setPosition(cameraPosition);
    setRotation(buildLookRotation(targetPosition - cameraPosition));
}

quaternion OrbitCameraSceneNode::buildLookRotation(const vector3df& lookDirection) {
    vector3df forward = lookDirection;
    const float forwardLength = glm::length(forward);
    if (forwardLength <= 0.000001f) {
        return quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }
    forward /= forwardLength;

    vector3df up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(forward, up)) > 0.999f) {
        up = vector3df(0.0f, 0.0f, 1.0f);
    }

    vector3df right = glm::cross(up, forward);
    const float rightLength = glm::length(right);
    if (rightLength <= 0.000001f) {
        return quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }
    right /= rightLength;
    up = glm::normalize(glm::cross(forward, right));

    return glm::normalize(glm::quat_cast(glm::mat3(right, up, forward)));
}
