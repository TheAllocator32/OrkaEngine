/*
 * File: BoundingShapes.hpp
 *
 * Contains AABB and BS
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../core/MathTypes.hpp"

// "Axis Aligned Bounding Box"
struct AABB {
    // Sets the AABB given a root point and size
    void setRootSize(vector3df root, vector3df size);

    // Sets the AABS given two corner points
    void setStartEnd(vector3df start, vector3df end);

    // Returns the absolute size of the AABB
    inline vector3df getSize() const { return maxEdge - minEdge; }

    // Returns the center point (average of min and max edges)
    inline vector3df getCenter() const { return (minEdge + maxEdge) * 0.5f; }

    // Returns min/max edges
    inline vector3df getMinEdge() const { return minEdge; }
    inline vector3df getMaxEdge() const { return maxEdge; }

    // Expands the AABB to include a given point
    void expandToInclude(const vector3df& point);
    
private:
    vector3df minEdge;
    vector3df maxEdge;
};

// "Bounding Sphere"
struct BS {
    // Sets center and radius
    inline void set(const vector3df& center, float radius) {
        this->center = center;
        this->radius = radius;
    }

    // Returns center
    inline vector3df getCenter() const { return center; }
    // Returns radius
    inline float getRadius() const { return radius; }

    // Expands the local BS to include the given point
    void expandToInclude(const vector3df& point);

    // Expands the local BS to include another BS
    void expandToInclude(const BS& other);

private:
    vector3df center = vector3df(0.0f);
    float radius = 0.0f;
};

