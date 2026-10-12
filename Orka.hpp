/*
 * File: Orka.hpp
 *
 * Contains Orka Base functions like init and deinit. And is by the way the main-header that includes all subsystems
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

#ifdef ORKA_HEADLESS
#ifndef ORKA_DISABLE_AUDIO
#define ORKA_DISABLE_AUDIO
#endif
#ifndef ORKA_DISABLE_PHYSICS
#define ORKA_DISABLE_PHYSICS
#endif
#ifndef ORKA_DISABLE_GUI
#define ORKA_DISABLE_GUI
#endif
#ifndef ORKA_DISABLE_GAMEPLAY
#define ORKA_DISABLE_GAMEPLAY
#endif
#ifndef ORKA_DISABLE_SCRIPTING
#define ORKA_DISABLE_SCRIPTING
#endif
#ifndef ORKA_DISABLE_LUA
#define ORKA_DISABLE_LUA
#endif
#ifndef ORKA_DISABLE_ANGELSCRIPT
#define ORKA_DISABLE_ANGELSCRIPT
#endif
#ifndef ORKA_DISABLE_INPUT
#define ORKA_DISABLE_INPUT
#endif
#ifndef ORKA_DISABLE_RENDERER
#define ORKA_DISABLE_RENDERER
#endif
#ifndef ORKA_DISABLE_SCENE
#define ORKA_DISABLE_SCENE
#endif
#ifndef ORKA_DISABLE_WINDOW
#define ORKA_DISABLE_WINDOW
#endif
#endif

#ifndef ORKA_DISABLE_SCENE
#include "3d/SceneManager.hpp"
#include "3d/SceneNode.hpp"
#include "3d/Scene.hpp"
#include "3d/OrbitCameraSceneNode.hpp"
#include "3d/ThirdPersonCameraSceneNode.hpp"
#endif
#ifndef ORKA_DISABLE_AUDIO
#include "audio/AudioEngine.hpp"
#endif
#include "Configurator.hpp"
#ifndef ORKA_DISABLE_CRYPTO
#include "crypto/Crypto.hpp"
#endif
#include "whaleml/WhaleMLFile.hpp"
#include "essentials/HardwareJudger.hpp"
#include "essentials/Logger.hpp"
#include "essentials/ProgressManager.hpp"
#include "essentials/Threading.hpp"
#include "essentials/Timing.hpp"
#ifndef ORKA_DISABLE_GAMEPLAY
#include "gameplay/CharacterController.hpp"
#include "gameplay/GameplayManager.hpp"
#include "gameplay/GameMode.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/UpgradeHandler.hpp"
#endif
#ifndef ORKA_DISABLE_NETWORKING
#ifndef ORKA_DISABLE_LEVIATHAN
#include "network/leviathan/LeviathanServer.hpp"
#include "network/leviathan/Leviathan.hpp"
#endif
#endif
#ifndef ORKA_DISABLE_GUI
#include "gui/GUIManager.hpp"
#include "gui/Translator.hpp"
#include "gui/internal_widgets/AllWidgets.hpp"
#endif
#ifndef ORKA_DISABLE_INPUT
#include "input/InputHandler.hpp"
#endif
#ifndef ORKA_DISABLE_NETWORKING
#include "network/Network.hpp"
#include "network/NetworkGameplay.hpp"
#endif
#include "OrkaVersion.h"
#ifndef ORKA_DISABLE_PHYSICS
#include "physics/PhysicsEngine.hpp"
#endif
#ifndef ORKA_DISABLE_SCRIPTING
#include "scripting/ScriptEngine.hpp"
#endif
#ifndef ORKA_DISABLE_RENDERER
#include "renderer/Renderer.hpp"
#include "renderer/FileLoader.hpp"
#endif
#include "utilities/Tools.hpp"
#ifndef ORKA_DISABLE_WINDOW
#include "window/Window.hpp"
#endif
#ifndef ORKA_DISABLE_AUDIO
#include "audio/MusicManager.hpp"
#include "primitives/audio/Sound.hpp"
#endif
#include <functional>
#include <type_traits>

#if __cplusplus < 202302L
#error "Hmm... I don't know what you expected but Orka requires C++23!"
#endif

inline std::string g_appName; // The application name (used internally)

// This is a static class (because Games usually use one Engine Context at a time and else the subsystems will fight about their resources...)
class OrkaEngine {
public:
    // Initialisation of all possible systems
    static void init(std::string_view gamename);
    // Deinitializes Orka systems that were initialized via init()
    static void deinit();

    // Shows Orka logo and plays emblem
    static void intro();

    // Shuts down with exit code
    static void shutdown(int code);

    // Sets functions that will be called when Orka is starting, updating and stopping (similar to MiscTools::schedule but this here is permanent not just one time!) The delta time used in update is in SECONDS!
    inline static void setExternalInitFunction(std::function<void()> func) { sm_appExtInit = func; }
    inline static void setExternalUpdateFunction(std::function<void(float)> func) { sm_appExtUpdate = func; }
    inline static void setExternalDeinitFunction(std::function<void()> func) { sm_appExtShutdown = func; }

    // Call this in a loop!
    static void update(float deltaTime);
private:
    inline static bool sm_isInitialized;
    inline static std::function<void()> sm_appExtInit, sm_appExtShutdown;
    inline static std::function<void(float)> sm_appExtUpdate;
    inline static std::string sm_gameServerAddress;
};

