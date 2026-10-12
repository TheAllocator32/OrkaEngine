#ifndef ORKA_DISABLE_SCRIPTING
/*
 * File: internal_bindings/ModelSceneNode.cpp
 *
 * Registers the ModelSceneNode class to the ScriptEngine.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Bindings.hpp"
#include "3d/ModelSceneNode.hpp"
#include "utilities/Tools.hpp"
#ifndef ORKA_DISABLE_PHYSICS
#include "physics/PhysicsEngine.hpp"
#endif

namespace ScriptBindings {
    inline bool isAbsoluteScriptPath(std::string_view path) {
        return path.starts_with("/") || path.starts_with("\\") || (path.size() > 1 && path[1] == ':');
    }

    inline std::string resolvePathRelativeToCurrentScript(std::string_view path) {
        if (path.empty() || isAbsoluteScriptPath(path)) return std::string(path);
        const std::string scriptPath = ScriptEngine::getCurrentScriptPath();
        if (scriptPath.empty()) return std::string(path);
        return MiscTools::getPath(scriptPath) + "/" + std::string(path);
    }

    inline ModelSceneNode* createModelSceneNodeForScript() {
        return &ModelSceneNode::create();
    }

#ifndef ORKA_DISABLE_PHYSICS
    inline void modelSceneNodeMove(ModelSceneNode* self, const vector3df& newPosition, float time) {
        PhysicsEngine::move(self->getID(), newPosition, time);
    }

    inline void modelSceneNodeMoveN(ModelSceneNode* self, double x, double y, double z, double time) {
        PhysicsEngine::move(
            self->getID(),
            vector3df(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)),
            static_cast<float>(time)
        );
    }

    inline void modelSceneNodeApplyCentralImpulse(ModelSceneNode* self, const vector3df& impulse) {
        PhysicsEngine::applyCentralImpulse(self->getID(), impulse);
    }

    inline void modelSceneNodeApplyCentralImpulseN(ModelSceneNode* self, double x, double y, double z) {
        PhysicsEngine::applyCentralImpulse(
            self->getID(),
            vector3df(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))
        );
    }
#else
    inline void modelSceneNodeMove(ModelSceneNode* self, const vector3df& newPosition, float time) {
        (void)time;
        self->setPosition(newPosition);
    }

    inline void modelSceneNodeMoveN(ModelSceneNode* self, double x, double y, double z, double time) {
        (void)time;
        self->setPosition(x, y, z);
    }

    inline void modelSceneNodeApplyCentralImpulse(ModelSceneNode* self, const vector3df& impulse) {
        (void)self;
        (void)impulse;
    }

    inline void modelSceneNodeApplyCentralImpulseN(ModelSceneNode* self, double x, double y, double z) {
        (void)self;
        (void)x;
        (void)y;
        (void)z;
    }
#endif

    inline void modelSceneNodeSetPositionN(ModelSceneNode* self, double x, double y, double z) {
        self->setPosition(x, y, z);
    }

    inline void modelSceneNodeSetRotationN(ModelSceneNode* self, double w, double x, double y, double z) {
        self->setRotation(w, x, y, z);
    }

    inline void modelSceneNodeSetScaleN(ModelSceneNode* self, double x, double y, double z) {
        self->setScale(x, y, z);
    }

    inline vector3df modelSceneNodeGetPosition(const ModelSceneNode* self) {
        return self->getPosition();
    }

    inline quaternion modelSceneNodeGetRotation(const ModelSceneNode* self) {
        return self->getRotation();
    }

    inline vector3df modelSceneNodeGetScale(const ModelSceneNode* self) {
        return self->getScale();
    }

    inline bool modelSceneNodeIsVisible(const ModelSceneNode* self) {
        return self->isVisible();
    }

    inline void modelSceneNodeSetPosition(ModelSceneNode* self, const vector3df& position) {
        self->setPosition(position);
    }

    inline void modelSceneNodeSetRotation(ModelSceneNode* self, const quaternion& rotation) {
        self->setRotation(rotation);
    }

    inline void modelSceneNodeSetScale(ModelSceneNode* self, const vector3df& scale) {
        self->setScale(scale);
    }

    inline void modelSceneNodeSetVisible(ModelSceneNode* self, bool visible) {
        self->setVisible(visible);
    }

    inline void modelSceneNodeDestroy(ModelSceneNode* self) {
        self->destroy();
    }

    inline void modelSceneNodeSetTag(ModelSceneNode* self, std::string_view tag) {
        self->setCustomUserData(tag);
    }

    inline std::string modelSceneNodeGetTag(const ModelSceneNode* self) {
        return std::string(self->getCustomUserData());
    }

    inline void modelSceneNodeLoadGLB(ModelSceneNode* self, std::string_view modelFile) {
        const std::string resolvedPath = resolvePathRelativeToCurrentScript(modelFile);
#ifdef ORKA_DISABLE_PHYSICS
        self->loadGLB(resolvedPath);
#else
        self->loadGLB(resolvedPath, PBT_RIGID, "convexhull", 1.0f);
#endif
    }

#ifndef ORKA_DISABLE_PHYSICS
    inline ModelSceneNodePhysicsBodyType modelSceneNodeParseBodyType(std::string_view bodyType) {
        if (bodyType == "soft" || bodyType == "Soft" || bodyType == "SOFT") return PBT_SOFT;
        if (bodyType == "ghost" || bodyType == "Ghost" || bodyType == "GHOST") return PBT_GHOST;
        return PBT_RIGID;
    }

    inline void modelSceneNodeLoadGLBWithPhysics(ModelSceneNode* self, std::string_view modelFile, std::string_view bodyType, std::string_view bodyShape, float mass) {
        self->loadGLB(resolvePathRelativeToCurrentScript(modelFile), modelSceneNodeParseBodyType(bodyType), bodyShape, mass);
    }
#endif

    inline void modelSceneNodeLoadFile(ModelSceneNode* self, std::string_view scenenodeFile) {
        self->loadFile(resolvePathRelativeToCurrentScript(scenenodeFile));
    }

    void registerModelSceneNode() {
        ScriptEngine::beginClass<ModelSceneNode>("ModelSceneNode");
        ScriptEngine::addConstructor<ModelSceneNode>(&createModelSceneNodeForScript);

        ScriptEngine::addConstMethod<ModelSceneNode, vector3df>("getPosition", &modelSceneNodeGetPosition);
        ScriptEngine::addConstMethod<ModelSceneNode, quaternion>("getRotation", &modelSceneNodeGetRotation);
        ScriptEngine::addConstMethod<ModelSceneNode, vector3df>("getScale", &modelSceneNodeGetScale);
        ScriptEngine::addConstMethod<ModelSceneNode, bool>("isVisible", &modelSceneNodeIsVisible);
        ScriptEngine::addMethod<ModelSceneNode, void, const vector3df&, float>("move", &modelSceneNodeMove);
        ScriptEngine::addMethod<ModelSceneNode, void, const vector3df&>("applyCentralImpulse", &modelSceneNodeApplyCentralImpulse);
        ScriptEngine::addMethodAngelScript<ModelSceneNode, void, double, double, double, double>("move", &modelSceneNodeMoveN);
        ScriptEngine::addMethodAngelScript<ModelSceneNode, void, double, double, double>("applyCentralImpulse", &modelSceneNodeApplyCentralImpulseN);

        ScriptEngine::addMethod<ModelSceneNode, void, const vector3df&>("setPosition", &modelSceneNodeSetPosition);
        ScriptEngine::addMethod<ModelSceneNode, void, const quaternion&>("setRotation", &modelSceneNodeSetRotation);
        ScriptEngine::addMethod<ModelSceneNode, void, const vector3df&>("setScale", &modelSceneNodeSetScale);
        ScriptEngine::addMethod<ModelSceneNode, void, bool>("setVisible", &modelSceneNodeSetVisible);
        ScriptEngine::addMethod<ModelSceneNode, void>("destroy", &modelSceneNodeDestroy);
        ScriptEngine::addMethodAngelScript<ModelSceneNode, void, double, double, double>("setPosition", &modelSceneNodeSetPositionN);
        ScriptEngine::addMethodAngelScript<ModelSceneNode, void, double, double, double, double>("setRotation", &modelSceneNodeSetRotationN);
        ScriptEngine::addMethodAngelScript<ModelSceneNode, void, double, double, double>("setScale", &modelSceneNodeSetScaleN);

        ScriptEngine::addMethodLua<ModelSceneNode, void, double, double, double>("setPositionN", &modelSceneNodeSetPositionN);
        ScriptEngine::addMethodLua<ModelSceneNode, void, double, double, double, double>("setRotationN", &modelSceneNodeSetRotationN);
        ScriptEngine::addMethodLua<ModelSceneNode, void, double, double, double>("setScaleN", &modelSceneNodeSetScaleN);
        ScriptEngine::addMethodLua<ModelSceneNode, void, double, double, double, double>("moveN", &modelSceneNodeMoveN);
        ScriptEngine::addMethodLua<ModelSceneNode, void, double, double, double>("applyCentralImpulseN", &modelSceneNodeApplyCentralImpulseN);

        ScriptEngine::addMethod<ModelSceneNode, void, std::string_view>("setTag", &modelSceneNodeSetTag);
        ScriptEngine::addConstMethod<ModelSceneNode, std::string>("getTag", &modelSceneNodeGetTag);

        ScriptEngine::addMethod<ModelSceneNode, void, std::string_view>("loadFile", &modelSceneNodeLoadFile);
        ScriptEngine::addMethod<ModelSceneNode, void, std::string_view>("loadGLB", &modelSceneNodeLoadGLB);
#ifndef ORKA_DISABLE_PHYSICS
        ScriptEngine::addMethodAngelScript<ModelSceneNode, void, std::string_view, std::string_view, std::string_view, float>("loadGLB", &modelSceneNodeLoadGLBWithPhysics);
        ScriptEngine::addMethodLua<ModelSceneNode, void, std::string_view, std::string_view, std::string_view, float>("loadGLBWithPhysics", &modelSceneNodeLoadGLBWithPhysics);
#endif

        ScriptEngine::endClass<ModelSceneNode>();
    }
}

#endif

