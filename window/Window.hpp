/*
 * File: Window.hpp
 *
 * Contains the Window class that creates manages and destroys the render window
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../primitives/core/MathTypes.hpp"
#include <string_view>

struct SDL_Window; // I don't want to include SDL.h here!

// Static class because Orka only supports a single window
class Window {
public:
    // Creates the Window using OrkaConfig configuration
    static void create(std::string_view title);
    static void create(std::string_view title, vector2di size, bool fullscreen = false, bool resizable = true, bool maximizable = true);
    // Destroys the window (Renderer hates this!)
    static void destroy();

    // Manually maximizes window
    static void maximize();
    // Manually minimizes/restores window
    static void minimize();
    static void restore();
    // Manually sets full screen for the current session
    static void setFullscreen(bool fullscreen);

    // Pumps SDL window/input/controller events
    static void update();
    static bool isOpen();

    // Gives renderer access to the low-level SDL window object... Used by Renderer
    static inline SDL_Window* getSDLWindow() { return sm_window; }

    // Sets the window size!
    static void resize();
private:
    inline static SDL_Window* sm_window;
    inline static bool sm_isOpen = false;
};

