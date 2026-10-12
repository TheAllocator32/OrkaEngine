/*
 * File: Orka.cpp
 *
 * Contains the implementation of Orka.hpp - This is the dirtiest file in Orka because it connects everythiing
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Orka.hpp"
#ifndef ORKA_HEADLESS
#include "3d/ModelSceneNode.hpp"
#include <SDL3/SDL.h>
#endif
#ifndef ORKA_DISABLE_SCRIPTING
#include "scripting/internal_bindings/Bindings.hpp"
#endif
#include <filesystem>

void OrkaEngine::init(std::string_view gameName) {
    std::string mode;
#ifdef ORKA_HEADLESS
    mode = " in headless mode";
#endif
    Logger::log(LET_SPECIAL, "Orka", "Initializing Orka Engine '" + std::string(ORKA_VERSION_CODENAME) + "' " + std::to_string(ORKA_VERSION_SERIES) + "." + std::to_string(ORKA_VERSION_NUMBER) + (ORKA_IS_ALPHA ? " in alpha stage (early development, probably broken as hell)" : ORKA_IS_BETA ? " in beta stage (might contain bugs and dragons)" : " stable build") + mode);
    g_appName = gameName;
    //std::atexit(OrkaEngine::shutdown); // Todo: Find workaround
    // Init Engine parts
#ifndef ORKA_HEADLESS
    Logger::log(LET_INFO, "Orka, Renderer", "Initializing renderer...");
    Window::create(g_appName);
    Renderer::init(RST_AUTO);
#endif
    Logger::log(LET_INFO, "Orka, Threading", "Initializing threading...");
    Threading::getMainThreadID(); // Set the Main Thread ID
    Logger::log(LET_INFO, "Orka, Progress", "Initializing progress manager...");
    ProgressManager::init();
#ifndef ORKA_DISABLE_AUDIO
    Logger::log(LET_INFO, "Orka, Audio", "Initializing audio engine...");
    AudioEngine::init();
#endif
#ifndef ORKA_DISABLE_NETWORKING
    Logger::log(LET_INFO, "Orka, Networking", "Initializing network...");
    RealtimeNetwork::init();
#endif
#ifndef ORKA_DISABLE_GUI
    Logger::log(LET_INFO, "Orka, GUI", "Initializing GUI...");
    GUIManager::init();
    GUIManager::rebuildFonts();
#endif
#ifndef ORKA_DISABLE_SCRIPTING
    Logger::log(LET_INFO, "Orka, Scripting", "Initializing script engine...");
    ScriptEngine::init();
    ScriptBindings::registerEngineProfiles();
    ScriptEngine::setDefaultBindingProfile("full");
#endif

    std::filesystem::create_directories(MiscTools::getGameUserDataPath());
    if (sm_appExtInit) {
        sm_appExtInit();
    }
    // Instantiate/create Engine parts
#ifndef ORKA_HEADLESS
    Logger::log(LET_INFO, "Orka, Scene", "Creating scene manager...");
    SceneManager::create();
#ifndef ORKA_DISABLE_PHYSICS
    Logger::log(LET_INFO, "Orka, Physics Engine", "Starting physics thread...");
    PhysicsEngine::startPhysicsThread();
#endif
    CameraSceneNode::create(false);
#endif
    sm_isInitialized=true;
}

void OrkaEngine::intro() {
    if (!sm_isInitialized) {
        Logger::log(LET_ERROR, "Orka", "Intro called but the system is not initialized! call init first!"); return;
    }
#ifdef ORKA_DISABLE_AUDIO
    Logger::log(LET_WARN, "Orka", "Intro requested, but ORKA_DISABLE_AUDIO is enabled.");
#else
    Sound& introSound = Sound::create();
#if defined(ORKA_IS_ALPHA) || defined(ORKA_IS_BETA)
    introSound.load("orka/OrkaEmblemUnstable.ogg");
#else
    introSound.load("orka/OrkaEmblem.ogg");
#endif
    introSound.play();
#endif
}

void OrkaEngine::deinit() {
    if (!sm_isInitialized) {
        return;
    }

#ifndef ORKA_DISABLE_GAMEPLAY
    GameplayManager::unloadCurrentGameMode();
#endif
#ifndef ORKA_DISABLE_GUI
    GUIManager::unloadCurrentRMLGUIPage();
    GUIManager::unloadCurrentGUIPage();
#endif

#ifndef ORKA_HEADLESS
    SceneManager::clear();
    ModelSceneNode::clearModelCaches();
#endif
#ifndef ORKA_DISABLE_SCRIPTING
    ScriptEngine::deinit();
#endif
#ifndef ORKA_DISABLE_NETWORKING
    RealtimeNetwork::deinit();
#endif
#ifndef ORKA_DISABLE_AUDIO
    AudioEngine::deinit();
#endif
#ifndef ORKA_HEADLESS
    Renderer::deinit();
    if (Window::isOpen()) {
        Window::destroy();
    }
#endif
    sm_isInitialized = false;
}

void OrkaEngine::shutdown(int code) {
    Logger::log(LET_INFO, "Orka", "Shutting down...");
    deinit();
    if (!sm_appExtShutdown) return;
    sm_appExtShutdown();
    std::exit(code);
}

void OrkaEngine::update(float deltaTime) {
#ifdef ORKA_HEADLESS
    Scheduler::workQueue();
    if (sm_appExtUpdate) sm_appExtUpdate(deltaTime);
    return;
#else
    SceneManager::flushDestroyedSceneNodes();
    SceneManager::update(deltaTime);
#ifndef ORKA_DISABLE_AUDIO
    AudioEngine::update(deltaTime);
#endif
    Scheduler::workQueue();
#ifndef ORKA_DISABLE_GUI
    GUIManager::update(deltaTime);
#endif
    if (sm_appExtUpdate) sm_appExtUpdate(deltaTime);
#ifndef ORKA_DISABLE_GAMEPLAY
    GameplayManager::update(deltaTime);
#endif
    Window::update();
    if (!Window::isOpen()) {
        SceneManager::flushDestroyedSceneNodes();
        return;
    }
    Renderer::frame();
    SceneManager::flushDestroyedSceneNodes();
#endif
}

