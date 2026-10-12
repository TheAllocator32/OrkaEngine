/*
 * File: CameraSceneNode.hpp
 *
 * Defines CameraSceneNode class!
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

class CameraSceneNode : public CreatableSceneNode<CameraSceneNode> {
public:
    // Creates a new CameraSceneNode or returns the currently active one.
    static CameraSceneNode& create(bool createNew=true, bool setAsActiveCamera=true);

    // Sets vertical FoV in degrees
    inline void setFoV(degrees fov) { m_fov = fov; }
    // Returns vertical FoV in degrees
    inline degrees getFoV() const { return m_fov; }

    // Sets the nearest distance before clipping (near plane)
    inline void setNearPlane(float nearPlane) { m_nearPlane = nearPlane; }
    // Returns the nearest distance before clipping (near plane)
    inline float getNearPlane() const { return m_nearPlane; }

    // Sets the farthest distance before clipping (far plane)
    inline void setFarPlane(float farPlane) { m_farPlane = farPlane; }
    // Returns the farthest distance before clipping (far plane)
    inline float getFarPlane() const { return m_farPlane; }

    // Returns the normalized forward direction of the camera
    vector3df getLookDirection() const;

    // Destroys this camera without using regular SceneNode removal
    void destroy() override;

    inline SceneNodeType getType() const override { return SNT_CAMERA; };
protected:
    friend class SceneManager;
    friend class SceneNode;
    inline CameraSceneNode(bool registerInSceneManager = false) : CreatableSceneNode(registerInSceneManager) {}
    ~CameraSceneNode() override;

private:
    degrees m_fov = 60.0f;
    float m_nearPlane = 0.01f;
    float m_farPlane = 10000.0f;
};

