/*
 * File: Model.hpp
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
#include "../physics/BoundingShapes.hpp"
#include "Geometry.hpp"
#include "Skeleton.hpp"
#include "../renderer/Material.hpp"
#include <unordered_map>
#include <utility>
#include <vector>

class Renderer; // Forward decl :/

class Model {
public:
    Model() = default;
    ~Model();

    // Empties the model
    void clear();

    // Returns all meshes
    inline std::vector<Mesh*> getMeshes() const { return m_meshes; }

    // Returns all materials for a given mesh
    inline std::vector<Material*> getMaterials(Mesh* mesh) const {
        auto it = m_materials.find(mesh);
        return it != m_materials.end() ? it->second : std::vector<Material*>();
    }

    // Returns the Skeleton
    inline Skeleton* getSkeleton() const { return m_skeleton; }

    // Returns the Bounding Box
    AABB getAABB() const;
    // Returns the Bounding Sphere
    BS getBS() const;

    // Clones the Model... Damn I hate writing that kind of unnecessary docs
    Model* clone() const;

    // Applies shading mode (see ModelShadingMode at the beginning of the file for more details). Will replace the shader of your selected model only!
    void setShadingMode(ModelShadingMode mode, Mesh* mesh);
    // Applies shading mode (see ModelShadingMode at the beginning of the file for more details). Will replace all currently set shaders!
    void setShadingMode(ModelShadingMode mode);

    inline void addMesh(Mesh* mesh) { m_meshes.push_back(mesh); m_dirty = true; }
    inline void addMesh(Mesh mesh) { m_meshes.push_back(new Mesh(std::move(mesh))); m_dirty = true; }
    inline void addMaterial(Mesh* mesh, Material* material) { m_materials[mesh].push_back(material); m_dirty = true; }
    inline void setSkeleton(Skeleton* skeleton) { if (m_skeleton) delete m_skeleton; m_skeleton = skeleton; m_dirty = true; }
    inline void markDirty() { m_dirty = true; }
    inline void clearDirty() { m_dirty = false; }
    bool isDirty() const;
private:
    friend class Renderer; // Renderer has to access m_materials
    std::vector<Mesh*> m_meshes;
    std::unordered_map<Mesh*, std::vector<Material*>> m_materials;
    Skeleton* m_skeleton = nullptr; // A model may have one skeleton!
    bool m_dirty = false;
};

