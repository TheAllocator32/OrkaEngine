#ifndef ORKA_DISABLE_SCRIPTING
/*
 * File: internal_bindings/SceneNode.cpp
 *
 * This File registers the SceneNode class to the ScriptEngine
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */


#include "Bindings.hpp"
#include "3d/SceneNode.hpp"
#include "utilities/Tools.hpp"
#ifndef ORKA_DISABLE_PHYSICS
#include "physics/PhysicsEngine.hpp"
#endif

namespace ScriptBindings {
    inline SceneNode* createSceneNodeForScript() {
        return &SceneNode::create();
    }

#ifndef ORKA_DISABLE_PHYSICS
    inline void sceneNodeMove(SceneNode* self, const vector3df& newPosition, float time) {
        PhysicsEngine::move(self->getID(), newPosition, time);
    }

    inline void sceneNodeMoveN(SceneNode* self, double x, double y, double z, double time) {
        PhysicsEngine::move(
            self->getID(),
            vector3df(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)),
            static_cast<float>(time)
        );
    }

    inline void sceneNodeApplyCentralImpulse(SceneNode* self, const vector3df& impulse) {
        PhysicsEngine::applyCentralImpulse(self->getID(), impulse);
    }

    inline void sceneNodeApplyCentralImpulseN(SceneNode* self, double x, double y, double z) {
        PhysicsEngine::applyCentralImpulse(
            self->getID(),
            vector3df(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))
        );
    }
#else
    inline void sceneNodeMove(SceneNode* self, const vector3df& newPosition, float time) {
        (void)time;
        self->setPosition(newPosition);
    }

    inline void sceneNodeMoveN(SceneNode* self, double x, double y, double z, double time) {
        (void)time;
        self->setPosition(x, y, z);
    }

    inline void sceneNodeApplyCentralImpulse(SceneNode* self, const vector3df& impulse) {
        (void)self;
        (void)impulse;
    }

    inline void sceneNodeApplyCentralImpulseN(SceneNode* self, double x, double y, double z) {
        (void)self;
        (void)x;
        (void)y;
        (void)z;
    }
#endif

    inline void sceneNodeSetPositionN(SceneNode* self, double x, double y, double z) {
        self->setPosition(x, y, z);
    }

    inline void sceneNodeSetRotationN(SceneNode* self, double w, double x, double y, double z) {
        self->setRotation(w, x, y, z);
    }

    inline void sceneNodeSetScaleN(SceneNode* self, double x, double y, double z) {
        self->setScale(x, y, z);
    }
    void registerSceneNode() {
        ScriptEngine::beginClass<SceneNode>("SceneNode");
        ScriptEngine::addConstructor<SceneNode>(&createSceneNodeForScript);
        ScriptEngine::addConstMethod<SceneNode, vector3df>("getPosition", &SceneNode::getPosition);
        ScriptEngine::addConstMethod<SceneNode, quaternion>("getRotation", &SceneNode::getRotation);
        ScriptEngine::addConstMethod<SceneNode, vector3df>("getScale", &SceneNode::getScale);
        ScriptEngine::addConstMethod<SceneNode, bool>("isVisible", &SceneNode::isVisible);
        ScriptEngine::addMethod<SceneNode, void, const vector3df&, float>("move", &sceneNodeMove);
        ScriptEngine::addMethod<SceneNode, void, const vector3df&>("applyCentralImpulse", &sceneNodeApplyCentralImpulse);
        ScriptEngine::addMethodAngelScript<SceneNode, void, double, double, double, double>("move", &sceneNodeMoveN);
        ScriptEngine::addMethodAngelScript<SceneNode, void, double, double, double>("applyCentralImpulse", &sceneNodeApplyCentralImpulseN);

        ScriptEngine::addMethod<SceneNode, void, const vector3df&>("setPosition", &SceneNode::setPosition);
        ScriptEngine::addMethod<SceneNode, void, const quaternion&>("setRotation", &SceneNode::setRotation);
        ScriptEngine::addMethod<SceneNode, void, const vector3df&>("setScale", &SceneNode::setScale);
        ScriptEngine::addMethod<SceneNode, void, bool>("setVisible", &SceneNode::setVisible);
        ScriptEngine::addMethod<SceneNode, void>("destroy", &SceneNode::destroy);
        ScriptEngine::addMethodAngelScript<SceneNode, void, double, double, double>("setPosition", &SceneNode::setPosition);
        ScriptEngine::addMethodAngelScript<SceneNode, void, double, double, double, double>("setRotation", &SceneNode::setRotation);
        ScriptEngine::addMethodAngelScript<SceneNode, void, double, double, double>("setScale", &SceneNode::setScale);

        ScriptEngine::addMethodLua<SceneNode, void, double, double, double>("setPositionN", &sceneNodeSetPositionN);
        ScriptEngine::addMethodLua<SceneNode, void, double, double, double, double>("setRotationN", &sceneNodeSetRotationN);
        ScriptEngine::addMethodLua<SceneNode, void, double, double, double>("setScaleN", &sceneNodeSetScaleN);
        ScriptEngine::addMethodLua<SceneNode, void, double, double, double, double>("moveN", &sceneNodeMoveN);
        ScriptEngine::addMethodLua<SceneNode, void, double, double, double>("applyCentralImpulseN", &sceneNodeApplyCentralImpulseN);

        ScriptEngine::addMethod<SceneNode, void, std::string_view>("setTag", &SceneNode::setCustomUserData);
        ScriptEngine::addConstMethod<SceneNode, std::string_view>("getTag", &SceneNode::getCustomUserData);

        ScriptEngine::endClass<SceneNode>();
    }
}

#endif

