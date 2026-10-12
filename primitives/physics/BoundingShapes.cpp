/*
 * File: BoundingShapes.cpp
 *
 * Bounding shape primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "BoundingShapes.hpp"

#include <algorithm>
#include <cmath>

void AABB::setRootSize(vector3df root, vector3df size) {
    minEdge = root;
    maxEdge = root + size;
}

void AABB::setStartEnd(vector3df start, vector3df end) {
    minEdge.x = std::min(start.x, end.x);
    minEdge.y = std::min(start.y, end.y);
    minEdge.z = std::min(start.z, end.z);
    maxEdge.x = std::max(start.x, end.x);
    maxEdge.y = std::max(start.y, end.y);
    maxEdge.z = std::max(start.z, end.z);
}

void AABB::expandToInclude(const vector3df& point) {
    minEdge.x = std::min(minEdge.x, point.x);
    minEdge.y = std::min(minEdge.y, point.y);
    minEdge.z = std::min(minEdge.z, point.z);
    maxEdge.x = std::max(maxEdge.x, point.x);
    maxEdge.y = std::max(maxEdge.y, point.y);
    maxEdge.z = std::max(maxEdge.z, point.z);
}

void BS::expandToInclude(const vector3df& point) {
    vector3df toPoint = point - center;
    float distanceSquared = glm::dot(toPoint, toPoint);
    if (distanceSquared > radius * radius) {
        radius = std::sqrt(distanceSquared);
    }
}

void BS::expandToInclude(const BS& other) {
    vector3df diff = other.center - center;
    float dist = glm::length(diff);
    if (dist + other.radius <= radius)
        return;
    if (dist + radius <= other.radius) {
        center = other.center;
        radius = other.radius;
        return;
    }
    float newRadius = (dist + radius + other.radius) * 0.5f;
    if (dist > 0.00001f) {
        center += diff * ((newRadius - radius) / dist);
    }
    radius = newRadius;
}

