/*
 * File: ThirdPersonCameraSceneNode.hpp
 *
 * Defines a third person camera that follows objects
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

class ThirdPersonCameraSceneNode : public CreatableSceneNode<ThirdPersonCameraSceneNode, CameraSceneNode> {
public:
    // Creates the ThirdPersonCameraSceneNode and optionally assigns it as the active SceneManager camera
    static ThirdPersonCameraSceneNode& create(bool createNew=true, bool setAsActiveCamera=true);

    // Sets the SceneNode ID the camera follows
    void setFollowObject(Identifier objectID);
    // Disables following and keeps the current camera transform
    inline void clearFollowTarget() { m_followEnabled = false; m_followTargetNodeID = -1; }

    // Sets the offset from the followed object's position relative to its origin (can be used e.g to move the camera around the head instead of the center)
    void setFollowOffset(const vector3df& offset);
    // Disables the offset and follows the object's center point
    inline void clearFollowOffset() { m_followOffsetEnabled = false; m_collisionFrameCounter = 0; if (m_followEnabled) applyFollowTransform(); }
    // Returns the offset from the followed object's position
    inline vector3df getFollowOffset() const { return m_followOffset; }

    // Sets the camera distance from the follow target
    void setFollowDistance(float distance);
    // Returns the camera distance from the follow target
    inline float getFollowDistance() const { return m_followDistance; }

    // Sets whether walls shorten the camera distance to prevent clipping
    void setCollisionEnabled(bool enabled);
    // Returns whether camera collision is enabled
    inline bool isCollisionEnabled() const { return m_collisionEnabled; }

    // Sets the distance kept between the camera and a hit wall
    void setCollisionPadding(float padding);

    // Sets the number of frames per physics collision check
    void setCollisionCheckInterval(unsigned int frames);

    // Sets the vertical camera angle, optionally moving smoothly to it
    void setXAngle(radians angle, bool smooth=false);
    // Returns the current vertical camera angle
    inline radians getXAngle() const { return m_xAngle; }

    // Sets the horizontal camera angle, optionally moving smoothly to it
    void setYAngle(radians angle, bool smooth=false);
    // Returns the current horizontal camera angle
    inline radians getYAngle() const { return m_yAngle; }
private:
    friend class SceneManager;
    friend class SceneNode;
    inline ThirdPersonCameraSceneNode(bool registerInSceneManager = false) : CreatableSceneNode(registerInSceneManager) {}
    ~ThirdPersonCameraSceneNode() override = default;

    // Updates the camera position to follow its target
    void update(float delta) override;
    vector3df getFollowTargetPosition();
    void applyFollowTransform(bool checkCollision=false, float delta=0.0f);
    vector3df resolveCollision(const vector3df& targetPosition, const vector3df& desiredCameraPosition, bool checkCollision, float delta);
    float getFollowObjectClearance(const vector3df& targetPosition, const vector3df& cameraDirection) const;
    static void updateSmoothAngle(radians& angle, radians targetAngle, bool& smoothing, float delta);
    static quaternion buildLookRotation(const vector3df& lookDirection);

    bool m_followEnabled = false;
    Identifier m_followTargetNodeID;
    vector3df m_followTargetPosition = vector3df(0.0f, 0.0f, 0.0f);
    vector3df m_followOffset = vector3df(0.0f, 0.0f, 0.0f);
    bool m_followOffsetEnabled = false;
    float m_followDistance = 3.0f;
    bool m_collisionEnabled = true;
    float m_collisionPadding = 1.0f;
    float m_collisionDistance = 3.0f;
    float m_currentCollisionDistance = 3.0f;
    unsigned int m_collisionCheckInterval = 3;
    unsigned int m_collisionFrameCounter = 0;
    radians m_xAngle = radians(0.0f);
    radians m_targetXAngle = radians(0.0f);
    radians m_yAngle = degrees(-90.0f);
    radians m_targetYAngle = degrees(-90.0f);
    bool m_smoothXAngle = false;
    bool m_smoothYAngle = false;
};

