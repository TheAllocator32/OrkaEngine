/*
 * File: Bindings.hpp
 *
 * This File contains the Script Bindings API used to register Engine classes to AngelScript
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../ScriptEngine.hpp"

// The script Bindings namespace contains all local coded script bindings
namespace ScriptBindings {
    // Registers AngelScript standard add-ons shipped by the AngelScript SDK.
    void registerAngelScriptAddons(asIScriptEngine* engine);
    // Registers the Orka Base functions
    void registerOrkaBase();
    // Registers the SceneNode
    void registerSceneNode();
    // Registers the ModelSceneNode
    void registerModelSceneNode();
    // Registers the CharacterController
    void registerCharacterController();
    // Registers all bindings
    inline void registerEngine() {
        registerOrkaBase();
        registerSceneNode();
        registerModelSceneNode();
        registerCharacterController();
    }
    // Registers named binding profiles without making ScriptEngine know Orka classes.
    inline void registerEngineProfiles() {
        ScriptEngine::registerBindingProfile("core", &registerOrkaBase);
        ScriptEngine::registerBindingProfile("scene", []() {
            registerOrkaBase();
            registerSceneNode();
            registerModelSceneNode();
        });
        ScriptEngine::registerBindingProfile("full", &registerEngine);
    }
}

