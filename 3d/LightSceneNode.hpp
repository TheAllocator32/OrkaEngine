/*
 * File: ModelSceneNode.hpp
 *
 * A SceneNode containing a light source. Can be described using .light.whaleml
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
#include "primitives/3d/Light.hpp"

class Renderer; // As always

class LightSceneNode : public CreatableSceneNode<LightSceneNode> {
public:
    using CreatableSceneNode::create;

    // Loads a .light.whaleml file
    void loadFile(std::string_view file);

    // Manually creates LightSceneNode
    void createManual(std::string_view name, LightType type, Color color, float intensity, float range, std::string_view scriptPath, bool instanceScript=true, float outerCone=0, float innerCone=0, bool enableShadows=true, int shadowResolution=1024);

    // Returns internal light
    Light getLight() const { return m_light; }

    // Clones the LightSceneNode!
    LightSceneNode& clone() const;

    inline SceneNodeType getType() const override { return SNT_LIGHT; }

protected:
    friend class SceneNode;
    Light m_light;
};

