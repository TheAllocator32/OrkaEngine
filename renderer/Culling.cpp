/*
 * File: Culling.cpp
 *
 * Culling implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Culling.hpp"
#include <algorithm>
#include <cmath>

bool Culler::isSphereInCameraView(const BS& bounds, const matrix4x4f& transform, const CameraSceneNode& camera, float aspectRatio) {
    const vector3df center = vector3df(transform * vector4df(bounds.getCenter(), 1.0f));
    const float maxScale = std::max({
        glm::length(vector3df(transform[0])),
        glm::length(vector3df(transform[1])),
        glm::length(vector3df(transform[2]))
    });
    const float radius = bounds.getRadius() * maxScale;

    quaternion rotation = camera.getRotation();
    if (glm::length(rotation) > 0.000001f) {
        rotation = glm::normalize(rotation);
    } else {
        rotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const glm::mat3 rotationBasis = glm::mat3_cast(rotation);
    const vector3df forward = rotationBasis * vector3df(0.0f, 0.0f, 1.0f);
    const vector3df up = rotationBasis * vector3df(0.0f, 1.0f, 0.0f);
    const vector3df right = rotationBasis * vector3df(1.0f, 0.0f, 0.0f);
    const vector3df toCenter = center - camera.getPosition();
    const float depth = glm::dot(toCenter, forward);

    if (depth + radius < camera.getNearPlane() || depth - radius > camera.getFarPlane()) {
        return false;
    }
    if (depth <= radius) {
        return true;
    }

    const float halfHeight = depth * std::tan(glm::radians(static_cast<float>(camera.getFoV())) * 0.5f);
    const float halfWidth = halfHeight * std::max(aspectRatio, 0.001f);
    return std::abs(glm::dot(toCenter, right)) <= halfWidth + radius &&
        std::abs(glm::dot(toCenter, up)) <= halfHeight + radius;
}

