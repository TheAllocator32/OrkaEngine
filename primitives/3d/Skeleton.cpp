/*
 * File: Skeleton.cpp
 *
 * Skeleton primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Skeleton.hpp"

void Bone::updateTransform(matrix4x4 mainTransform) {
    if (m_localDirty) {
        matrix4x4 R = glm::mat4_cast(m_rotation);
        R[0] *= m_scale.x;
        R[1] *= m_scale.y;
        R[2] *= m_scale.z;
        matrix4x4 T = glm::translate(matrix4x4(1.0f), m_position);
        m_localTransform = T * R;
        m_localDirty = false;
    }
    const matrix4x4& parentGlobal = m_parent ? m_parent->m_globalTransform : mainTransform;
    if (m_globalDirty || (m_parent && m_parent->m_globalDirty)) {
        m_globalTransform = parentGlobal * m_localTransform;
        m_globalDirty = false;
    }
    for (Bone* c : m_children) {
        c->m_globalDirty = c->m_globalDirty || m_globalDirty;
        c->updateTransform(mainTransform);
    }
}

Bone* Bone::clone(Bone* parent) const {
    Bone* b = new Bone(parent);
    b->m_position = m_position;
    b->m_scale = m_scale;
    b->m_rotation = m_rotation;
    b->m_localTransform = m_localTransform;
    b->m_globalTransform = m_globalTransform;
    b->m_localDirty = m_localDirty;
    b->m_globalDirty = m_globalDirty;
    b->m_children.reserve(m_children.size());
    for (Bone* child : m_children) {
        if (!child) continue;
        b->m_children.push_back(child->clone(b));
    }
    return b;
}

void Skeleton::update() {
    if (m_localDirty) {
        matrix4x4 R = glm::mat4_cast(m_rotation);
        R[0] *= m_scale.x;
        R[1] *= m_scale.y;
        R[2] *= m_scale.z;
        matrix4x4 T = glm::translate(matrix4x4(1.0f), m_position);
        m_localTransform = T * R;
        m_localDirty = false;
    }
    if (m_rootBone) {
        m_rootBone->updateTransform(m_localTransform);
    }
}

Skeleton* Skeleton::clone() const {
    Skeleton* s = new Skeleton();
    if (s->m_rootBone) {
        delete s->m_rootBone;
        s->m_rootBone = nullptr;
    }
    s->m_rootBone = m_rootBone ? m_rootBone->clone(nullptr) : nullptr;
    s->m_localTransform = m_localTransform;
    s->m_position = m_position;
    s->m_scale = m_scale;
    s->m_rotation = m_rotation;
    s->m_localDirty = m_localDirty;
    return s;
}

