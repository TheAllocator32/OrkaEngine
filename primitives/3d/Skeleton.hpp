/*
 * File: Skeleton.hpp
 *
 * Defines the Bone and Skeleton structure
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
#include <vector>

struct Bone {
    matrix4x4 m_localTransform{};
    bool m_localDirty = false;

    inline Bone(Bone* parent = nullptr) : m_parent(parent), m_localDirty(true), m_globalDirty(true) {}

    // Set position/rotation/scale...
    inline void setPosition(vector3df p) { m_position = p; m_localDirty = true; m_globalDirty = true; }
    inline void setRotation(quaternion r) { m_rotation = glm::normalize(r); m_localDirty = true; m_globalDirty = true; }
    inline void setScale(vector3df s) { m_scale = s; m_localDirty = true; m_globalDirty = true; }

    inline void addChild(Bone* b) { m_children.push_back(b); }

    // Updates the transforms
    void updateTransform(matrix4x4 mainTransform = matrix4x4(1.0f));

    Bone* clone(Bone* parent = nullptr) const;
private:
    vector3df m_position;
    vector3df m_scale;      // These 3 values aren't meant to be the real rotation/position/scale (if you e.g. manually change the transforms!)
    quaternion m_rotation;
    Bone* m_parent = nullptr;
    std::vector<Bone*> m_children;
    matrix4x4 m_globalTransform;
    bool m_globalDirty = false;
};


class Skeleton {
public:
    inline Skeleton() : m_rootBone(new Bone()) {}
    inline ~Skeleton() { delete m_rootBone; }

    // Sets a Root Bone
    inline void setRootBone(Bone* rootBone) { m_rootBone = rootBone; }

    // Sets the position relative to the owning SceneNode!
    inline void setPosition(vector3df p) { m_position = p; m_localDirty = true; }
    // Sets the rotation relative to the owning SceneNode!
    inline void setRotation(quaternion r) { m_rotation = glm::normalize(r); m_localDirty = true; }
    // Sets the scale relative to the owning SceneNode!
    inline void setScale(vector3df s) { m_scale = s; m_localDirty = true; }

    void update();

    Skeleton* clone() const;
private:
    Bone* m_rootBone = nullptr;
    matrix4x4 m_localTransform;
    vector3df m_position;
    vector3df m_scale;
    quaternion m_rotation;
    bool m_localDirty = false;
};

