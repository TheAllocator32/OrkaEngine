/*
 * File: GUIRendererRMLUI.hpp
 *
 * Contains the RmlUi render lambda called by Renderer.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

#include "essentials/Identifier.hpp"
#include "renderer/RendererTypes.hpp"
#include <functional>
#include <string>

namespace Rml {
class Context;
class ElementDocument;
}

// RmlUi backend for Orka's bgfx renderer.
// Use registerWithRenderer() for automatic hookup, or pass getRendererLambda() to Renderer::addDrawFunction.
class GUIRendererRMLUI {
public:
    // Ensures RmlUi interfaces/context exist.
    static void ensureInitialized();
    static void shutdown();

    // Lambda for Renderer::addDrawFunction(...). This is executed every frame by Renderer.
    static std::function<void(float)> getRendererLambda();
    static Identifier registerWithRenderer(RenderQueueOrder order = RQO_3DSPACE);
    static void unregisterFromRenderer();

    // Called by GUIManager once per engine update.
    static void prepareFrame(float deltaInSeconds);

    // RmlUi API exposed through GUIManager.
    static bool loadFontFace(std::string_view fontPath, bool fallbackFace = false);
    static bool setCurrentDocument(std::string_view documentPath);
    static void clearCurrentDocument();
    static Rml::Context* getContext();
    static Rml::ElementDocument* getCurrentDocument();

private:
    static void renderTick(float deltaInSeconds);
};

