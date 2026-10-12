/*
 * File: ThirdPersonCameraSceneNode.cpp
 *
 * ThirdPersonCameraSceneNode Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "ThirdPersonCameraSceneNode.hpp"
#include "ModelSceneNode.hpp"
#include "SceneManager.hpp"
#ifndef ORKA_DISABLE_PHYSICS
#include "../physics/PhysicsEngine.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <numbers>

ThirdPersonCameraSceneNode& ThirdPersonCameraSceneNode::create(bool createNew, bool setAsActiveCamera) {
    if (SceneManager::hasCamera() && !createNew) {
        try {
            return SceneManager::getCamera().as<ThirdPersonCameraSceneNode>();
        }
        catch (const std::bad_cast&) {}
    }

    ThirdPersonCameraSceneNode& camera = SceneNode::create<ThirdPersonCameraSceneNode>(false);
    camera.setPosition(vector3df(0.0f, 0.0f, -3.0f));
    camera.setRotation(quaternion(1.0f, 0.0f, 0.0f, 0.0f));
    camera.setFoV(60.0f);
    camera.setNearPlane(0.05f);
    camera.setFarPlane(200.0f);
    if (setAsActiveCamera) SceneManager::setCamera(&camera);
    return camera;
}

void ThirdPersonCameraSceneNode::update(float delta) {
    updateScript(delta);
    updateSmoothAngle(m_xAngle, m_targetXAngle, m_smoothXAngle, delta);
    updateSmoothAngle(m_yAngle, m_targetYAngle, m_smoothYAngle, delta);
    if (!m_followEnabled) return;

    const bool checkCollision = m_collisionEnabled && m_collisionFrameCounter == 0;
    applyFollowTransform(checkCollision, delta);
    if (m_collisionEnabled) {
        m_collisionFrameCounter = (m_collisionFrameCounter + 1) % m_collisionCheckInterval;
    }
}

void ThirdPersonCameraSceneNode::setFollowObject(Identifier objectID) {
    m_followTargetNodeID = objectID;
    if (SceneManager::sceneNodeExists(objectID)) {
        m_followTargetPosition = SceneManager::getSceneNode(objectID).getPosition();
    }
    m_followEnabled = true;
    m_collisionDistance = std::max(m_followDistance, 0.0001f);
    m_currentCollisionDistance = m_collisionDistance;
    m_collisionFrameCounter = 0;
    applyFollowTransform();
}

void ThirdPersonCameraSceneNode::setFollowOffset(const vector3df& offset) {
    m_followOffset = offset;
    m_followOffsetEnabled = true;
    m_collisionFrameCounter = 0;
    if (m_followEnabled) applyFollowTransform();
}

void ThirdPersonCameraSceneNode::setFollowDistance(float distance) {
    m_followDistance = distance;
    m_collisionDistance = std::max(distance, 0.0001f);
    m_currentCollisionDistance = m_collisionDistance;
    m_collisionFrameCounter = 0;
    if (m_followEnabled) applyFollowTransform();
}

void ThirdPersonCameraSceneNode::setCollisionEnabled(bool enabled) {
    m_collisionEnabled = enabled;
    m_collisionDistance = std::max(m_followDistance, 0.0001f);
    m_currentCollisionDistance = m_collisionDistance;
    m_collisionFrameCounter = 0;
    if (m_followEnabled) applyFollowTransform();
}

void ThirdPersonCameraSceneNode::setCollisionPadding(float padding) {
    m_collisionPadding = std::max(padding, 0.0f);
    m_collisionDistance = std::max(m_followDistance, 0.0001f);
    m_currentCollisionDistance = m_collisionDistance;
    m_collisionFrameCounter = 0;
    if (m_followEnabled) applyFollowTransform();
}

void ThirdPersonCameraSceneNode::setCollisionCheckInterval(unsigned int frames) {
    m_collisionCheckInterval = std::max(frames, 1u);
    m_collisionFrameCounter = 0;
}

void ThirdPersonCameraSceneNode::setXAngle(radians angle, bool smooth) {
    m_targetXAngle = angle;
    m_smoothXAngle = smooth;
    if (!smooth) m_xAngle = angle;
    if (m_followEnabled) applyFollowTransform();
}

void ThirdPersonCameraSceneNode::setYAngle(radians angle, bool smooth) {
    m_targetYAngle = angle;
    m_smoothYAngle = smooth;
    if (!smooth) m_yAngle = angle;
    if (m_followEnabled) applyFollowTransform();
}

vector3df ThirdPersonCameraSceneNode::getFollowTargetPosition() {
    if (!SceneManager::sceneNodeExists(m_followTargetNodeID)) return m_followTargetPosition;
    const SceneNode* targetNode = &SceneManager::getSceneNode(m_followTargetNodeID);

    m_followTargetPosition = m_followOffsetEnabled
        ? targetNode->getPosition() + m_followOffset
        : targetNode->getCenterPoint();
    return m_followTargetPosition;
}

void ThirdPersonCameraSceneNode::applyFollowTransform(bool checkCollision, float delta) {
    const vector3df targetPosition = getFollowTargetPosition();
    const float safeDistance = std::max(m_followDistance, 0.0001f);
    const float horizontalDistance = std::cos(static_cast<float>(m_xAngle)) * safeDistance;
    const vector3df cameraOffset(
        std::cos(static_cast<float>(m_yAngle)) * horizontalDistance,
        std::sin(static_cast<float>(m_xAngle)) * safeDistance,
        std::sin(static_cast<float>(m_yAngle)) * horizontalDistance);
    const vector3df cameraPosition = resolveCollision(targetPosition, targetPosition + cameraOffset, checkCollision, delta);
    setPosition(cameraPosition);
    setRotation(buildLookRotation(targetPosition - cameraPosition));
}

vector3df ThirdPersonCameraSceneNode::resolveCollision(const vector3df& targetPosition, const vector3df& desiredCameraPosition, bool checkCollision, float delta) {
    if (!m_collisionEnabled) return desiredCameraPosition;

    const vector3df cameraOffset = desiredCameraPosition - targetPosition;
    const float desiredDistance = glm::length(cameraOffset);
    if (desiredDistance <= 0.000001f) return desiredCameraPosition;
    const vector3df cameraDirection = cameraOffset / desiredDistance;

#ifndef ORKA_DISABLE_PHYSICS
    if (checkCollision) {
        m_collisionDistance = desiredDistance;
        const std::vector<RaycastResult> hits = PhysicsEngine::raycastAll(targetPosition, desiredCameraPosition);
        for (const RaycastResult& hit : hits) {
            if (hit.sceneNode == m_followTargetNodeID) continue;

            const float hitDistance = hit.hitFraction * desiredDistance;
            m_collisionDistance = std::max(hitDistance - m_collisionPadding, 0.0001f);
            break;
        }
    }
#else
    (void)checkCollision;
    m_collisionDistance = desiredDistance;
#endif

    const float wallDistance = std::min(desiredDistance, m_collisionDistance);
    const float targetDistance = std::max(wallDistance, getFollowObjectClearance(targetPosition, cameraDirection));
    if (delta <= 0.0f) {
        m_currentCollisionDistance = targetDistance;
    } else {
        const float interpolation = 1.0f - std::exp(-10.0f * delta);
        m_currentCollisionDistance += (targetDistance - m_currentCollisionDistance) * interpolation;
    }
    return targetPosition + cameraDirection * m_currentCollisionDistance;
}

float ThirdPersonCameraSceneNode::getFollowObjectClearance(const vector3df& targetPosition, const vector3df& cameraDirection) const {
    if (!SceneManager::sceneNodeExists(m_followTargetNodeID)) return 0.0f;
    const SceneNode* targetNode = &SceneManager::getSceneNode(m_followTargetNodeID);
    if (targetNode->getType() != SNT_MODEL) return 0.0f;

    const BS targetBounds = targetNode->as<ModelSceneNode>().getBS();
    const float radius = targetBounds.getRadius() + getNearPlane();
    if (radius <= 0.0f) return 0.0f;

    const vector3df centerOffset = targetPosition - targetBounds.getCenter();
    const float projectedCenter = glm::dot(centerOffset, cameraDirection);
    const float discriminant = projectedCenter * projectedCenter - glm::dot(centerOffset, centerOffset) + radius * radius;
    if (discriminant <= 0.0f) return 0.0f;
    return std::max(-projectedCenter + std::sqrt(discriminant), 0.0f);
}

void ThirdPersonCameraSceneNode::updateSmoothAngle(radians& angle, radians targetAngle, bool& smoothing, float delta) {
    if (!smoothing) return;

    const float difference = std::remainder(static_cast<float>(targetAngle) - static_cast<float>(angle), std::numbers::pi_v<float> * 2.0f);
    if (std::abs(difference) <= 0.0001f) {
        angle = targetAngle;
        smoothing = false;
        return;
    }

    const float interpolation = 1.0f - std::exp(-10.0f * std::max(delta, 0.0f));
    angle = radians(static_cast<float>(angle) + difference * interpolation);
}

quaternion ThirdPersonCameraSceneNode::buildLookRotation(const vector3df& lookDirection) {
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
