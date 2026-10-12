/*
 * File: Culling.hpp
 *
 * Contains some culling algorithms. Currently only frustum culling
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../3d/CameraSceneNode.hpp"
#include "../primitives/core/MathTypes.hpp"
#include "../primitives/physics/BoundingShapes.hpp"

class Culler {
public:
    // Returns whether a transformed bounding sphere intersects the active camera view.
    static bool isSphereInCameraView(const BS& bounds, const matrix4x4f& transform, const CameraSceneNode& camera, float aspectRatio);
};

