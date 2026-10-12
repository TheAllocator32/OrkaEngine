/*
 * File: ModelSceneNode.hpp
 *
 * A SceneNode containing a mesh. Can be described using .model.whaleml
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "SceneNode.hpp"
#include "../primitives/3d/Model.hpp"
#include <memory>

#ifndef ORKA_DISABLE_PHYSICS
class PhysicsEngine; // If you don't use PhysicsEngine, this will be no problem. Just forward decl
enum ModelSceneNodePhysicsBodyType { PBT_RIGID, PBT_SOFT, PBT_GHOST };
#endif

class ModelSceneNode : public CreatableSceneNode<ModelSceneNode> {
public:
    using CreatableSceneNode::create;
    
    // Loads a .model.whaleml file
    void loadFile(std::string_view scenenodeFile);
    // Loads a raw GLB file without external stuff like script or .*body.whaleml!
#ifdef ORKA_DISABLE_PHYSICS
    void loadGLB(std::string_view modelFile);
#else
    void loadGLB(std::string_view modelFile, ModelSceneNodePhysicsBodyType bodyType=PBT_RIGID, std::string_view bodyShape="convexhull", float mass=1.0f);
#endif
    // Creates the SceneNode manually (not recommended)
    void createManual(std::string_view name, std::string_view rootpath, std::string_view mesh, std::string_view body, std::string_view bodySettings, std::string_view script, std::string_view shadertype, std::string_view shaderfile, bool instanceScript, bool skipFileLoading=false, bool skipPhysicsCreation=false);
    // Assigns a manually constructed model - Use with care and only if really, really necessary
    void setManualModel(Model* model);
    // Clears model reload/instance cache entries that are no longer in use.
    static void clearModelCaches();

    // Returns the underlying Orka Model
    inline Model& getModel() { if (m_model.use_count()!=1) m_model = std::shared_ptr<Model>(m_model->clone()); return *m_model; }
    inline const Model& getModel() const { return *m_model; }
    // Finds the underlying Orka Model (may be nullptr if not loaded)
    inline Model* findModel() { if (!m_model) return nullptr; if (m_model.use_count()!=1) m_model = std::shared_ptr<Model>(m_model->clone()); return m_model.get(); }
    inline const Model* findModel() const { return m_model.get(); }

    // Returns the transformed center of the model AABB
    vector3df getCenterPoint() const override;
    // Returns the world-space model AABB
    AABB getAABB() const;
    // Returns the world-space model bounding sphere
    BS getBS() const;

    // Clones the ModelSceneNode!
    ModelSceneNode& clone() const;

    inline SceneNodeType getType() const override { return SNT_MODEL; };

private:
    friend class SceneNode;

    std::string m_filepath, m_rootpath, m_mesh, m_body, m_bodySettings, m_shaderType, m_shaderFile;
    WhaleMLFile m_physicsSettingsWhaleMLFile = WhaleMLFile();
    bool m_fileLoaded = false;
    std::shared_ptr<Model> m_model; // I love shared_ptrs if you don't already know...
};

