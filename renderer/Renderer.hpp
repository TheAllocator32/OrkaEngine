/*
 * File: Renderer.hpp
 *
 * Orka's Renderer (based on bgfx)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../primitives/3d/Model.hpp"
#include "../primitives/renderer/RenderTarget.hpp"
#include "../essentials/Threading.hpp"
#include "../essentials/Identifier.hpp"
#include "RendererTypes.hpp"
#include <map>

// Orkas new Renderer! It's pretty thread-save
class Renderer {
public:
    // Initialize the Renderer
    static void init(RenderSystemType rst);
    // Kills the renderer
    static void deinit();

    // Called once per frame (From the main thread only!)
    static void frame();

    // Model upload/modification
    // Uploads Mesh - THREADSAFE
    static void upload(const Model* model);

    // Sets the render target used by the main scene view.
    static void setRenderTarget(RenderTarget* renderTarget);
    // Resets the main scene view back to the default render target.
    static void resetRenderTarget();

    // Render queue
    // Adds a custom "draw function" that will be executed whenever a new frame is drawn (in a specific order). Very useful for UI since you may execute custom bgfx commands. By selecting "isThreadsafe" your function may be called outside the Render thread! The signature of that function must be void (float deltaInSeconds). Returns ID. THREADSAFE
    static Identifier addDrawFunction(std::function<void(float)> function, RenderQueueOrder order, bool isThreadsafe);
    static void removeDrawFunction(Identifier id);

    // Utilities
    // Returns the used RenderSystem
    static RenderSystemType getRenderSystemType() { return sm_usedRenderSystem; }
    // Returns the requested Orka render system before bgfx collapses variants like OpenGL2/OpenGL4.
    static RenderSystemType getRequestedRenderSystemType() { return sm_requestedRenderSystem; }
    // Returns the current projection matrix used for the scene view
    static matrix4x4 getProjectionMatrix() { return sm_projectionMatrix; }
    // Returns the current view matrix used for the scene view
    static matrix4x4 getViewMatrix() { return sm_viewMatrix; }
private:
    inline static RenderSystemType sm_usedRenderSystem;
    inline static RenderSystemType sm_requestedRenderSystem = RST_AUTO;
    inline static bool sm_isInitialized;
    inline static bool sm_skipSMAARecreateUntilNextFrame = false;
    inline static uint32_t sm_backbufferWidth, sm_backbufferHeight, sm_resetFlags;
    inline static matrix4x4 sm_projectionMatrix = matrix4x4(1.0f);
    inline static matrix4x4 sm_viewMatrix = matrix4x4(1.0f);
    inline static threadsafe<int> sm_nextModelID;
    inline static threadsafe<Identifier> sm_nextDrawFunctionID = threadsafe<Identifier>(Identifier(0));
    struct DrawEntry { Identifier id; std::function<void(float)> fn; bool ready; };
    inline static threadsafe<std::map<RenderQueueOrder, std::vector<DrawEntry>>> sm_drawFunctions;
};

