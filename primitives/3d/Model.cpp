/*
 * File: Model.cpp
 *
 * Model primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Model.hpp"

Model::~Model() {
    clear();
}

void Model::clear() {
    for (Mesh* m : m_meshes) delete m;
    m_meshes.clear();
    for (auto& kv : m_materials) {
        for (auto& m : kv.second) delete m;
        kv.second.clear();
    }
    m_materials.clear();
    if (m_skeleton) delete m_skeleton;
    m_skeleton = nullptr;
    m_dirty = false;
}

AABB Model::getAABB() const {
    bool hasBounds = false;
    AABB aabb;
    for (Mesh* m : m_meshes) {
        if (!m) continue;
        const AABB meshAABB = m->getAABB();
        if (!hasBounds) {
            aabb = meshAABB;
            hasBounds = true;
            continue;
        }
        aabb.expandToInclude(meshAABB.getMaxEdge());
        aabb.expandToInclude(meshAABB.getMinEdge());
    }
    return aabb;
}

BS Model::getBS() const {
    bool hasBounds = false;
    BS bs;
    for (Mesh* m : m_meshes) {
        if (!m) continue;
        const BS meshBS = m->getBS();
        if (!hasBounds) {
            bs = meshBS;
            hasBounds = true;
        } else {
            bs.expandToInclude(meshBS);
        }
    }
    return bs;
}

Model* Model::clone() const {
    Model* newModel = new Model();
    std::unordered_map<Mesh*, Mesh*> mesh2NewMeshMap;
    for (Mesh* m : m_meshes) {
        if (m == nullptr) continue;
        Mesh* newMesh = m->clone();
        mesh2NewMeshMap[m] = newMesh;
        newModel->addMesh(newMesh);
    }
    for (const auto& kv : m_materials) {
        auto newMeshIt = mesh2NewMeshMap.find(kv.first);
        if (newMeshIt == mesh2NewMeshMap.end()) continue;
        for (auto& m : kv.second) {
            newModel->addMaterial(newMeshIt->second, m ? m->clone() : nullptr);
        }
    }
    if (m_skeleton) {
        newModel->setSkeleton(m_skeleton->clone());
    }
    newModel->m_dirty = m_dirty;
    return newModel;
}

void Model::setShadingMode(ModelShadingMode mode, Mesh* mesh) {
    auto& materials = m_materials[mesh];
    for (auto material : materials) {
        material->setShadingMode(mode);
    }
}

void Model::setShadingMode(ModelShadingMode mode) {
    for (auto m : m_meshes) {
        setShadingMode(mode, m);
    }
}

bool Model::isDirty() const {
    if (m_dirty) return true;
    for (Mesh* m : m_meshes) {
        if (m != nullptr && m->isDirty()) return true;
    }
    return false;
}

