/*
 * File: OrbitCameraSceneNode.hpp
 *
 * Defines OrbitCameraSceneNode class!
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "CameraSceneNode.hpp"

enum CameraOrbitDirection {
    COD_HORIZONTAL,
    COD_VERTICAL
};

class OrbitCameraSceneNode : public CreatableSceneNode<OrbitCameraSceneNode, CameraSceneNode> {
public:
    // Creates the OrbitCameraSceneNode and optionally assigns it as the active SceneManager camera
    static OrbitCameraSceneNode& create(bool createNew=true, bool setAsActiveCamera=true);

    // Sets a point the camera orbits around
    void setOrbitPoint(const vector3df& point);
    // Sets the SceneNode ID the camera orbits around
    void setOrbitObject(Identifier objectID);

    // Disables the orbit mode and keeps the current camera transform
    inline void clearOrbitTarget() { m_orbitEnabled = false; m_orbitTargetNodeID = -1; }

    // Sets whether the orbit runs on the horizontal or vertical plane
    void setOrbitDirection(CameraOrbitDirection direction);
    // Returns whether the orbit runs on the horizontal or vertical plane
    inline CameraOrbitDirection getOrbitDirection() const { return m_orbitDirection; }

    // Sets the orbit distance from the target
    void setOrbitDistance(float distance);
    // Returns the orbit distance from the target
    inline float getOrbitDistance() const { return m_orbitDistance; }

    // Sets the height offset used when orbiting horizontally
    void setOrbitHeight(float height);
    // Returns the height offset used when orbiting horizontally
    inline float getOrbitHeight() const { return m_orbitHeight; }

    // Sets the breadth offset used when orbiting vertically
    void setOrbitBreadth(float breadth);
    // Returns the breadth offset used when orbiting vertically
    inline float getOrbitBreadth() const { return m_orbitBreadth; }

    // Sets the orbit speed in radians per second
    inline void setOrbitSpeed(radians speed) { m_orbitSpeed = speed; }
    // Returns the orbit speed in radians per second
    inline radians getOrbitSpeed() const { return m_orbitSpeed; }

    // Sets the current orbit angle in radians
    void setOrbitAngle(radians angle);
    // Returns the current orbit angle in radians
    inline radians getOrbitAngle() const { return m_orbitAngle; }
private:
    friend class SceneManager;
    friend class SceneNode;
    inline OrbitCameraSceneNode(bool registerInSceneManager = false) : CreatableSceneNode(registerInSceneManager) {}
    ~OrbitCameraSceneNode() override = default;

    // Updates the camera and applies enabled orbit movement
    void update(float delta) override;
    vector3df getOrbitTargetPosition() const;
    void applyOrbitTransform();
    static quaternion buildLookRotation(const vector3df& lookDirection);

    bool m_orbitEnabled = false;
    Identifier m_orbitTargetNodeID;
    vector3df m_orbitTargetPoint = vector3df(0.0f, 0.0f, 0.0f);
    CameraOrbitDirection m_orbitDirection = COD_HORIZONTAL;
    float m_orbitDistance = 3.0f;
    float m_orbitHeight = 0.0f;
    float m_orbitBreadth = 0.0f;
    radians m_orbitSpeed = radians(1.0f);
    radians m_orbitAngle = radians(0.0f);
};

