/*
* File: GUIRendererImGUI.hpp
 *
 * Contains the ImGUI render lambda called by Renderer
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
#include "primitives/renderer/Shader.hpp"
#include "renderer/RendererTypes.hpp"
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <functional>
#include <imgui.h>

struct GUIRendererImGUIState;

// Dear ImGui backend for Orka's bgfx Renderer.
// Use registerWithRenderer() for automatic hookup, or pass getRendererLambda() to Renderer::addDrawFunction.
class GUIRendererImGUI {
public:
    // Ensures ImGui context/platform state exist before GUIManager uses ImGui APIs.
    static void ensureInitialized();
    // Best-effort cleanup for SDL hooks/resources.
    static void shutdown();

    // Lambda for Renderer::addDrawFunction(...). This is executed every frame by Renderer.
    static std::function<void(float)> getRendererLambda();
    // Convenience registration helper (non-threadsafe, main-thread only).
    static Identifier registerWithRenderer(RenderQueueOrder order = RQO_3DSPACE);
    static void unregisterFromRenderer();

    // Rebuilds and uploads font atlas on the next frame.
    static void rebuildFontAtlas();
    // Ensures ImGui::NewFrame() has been called for this tick.
    static void prepareFrame(float deltaInSeconds);

    // Shared BGFX pipeline used by ImGui and compatible UI backends (eg. RmlUi).
    // Valid only after ensureSharedRenderResources() returned true.
    static bool ensureSharedRenderResources();
    static bgfx::ProgramHandle getSharedProgram();
    static bgfx::UniformHandle getSharedTextureUniform();
    static const bgfx::VertexLayout& getSharedVertexLayout();

private:
    static const char* getClipboardText(void* userData);
    static void setClipboardText(void* userData, const char* text);
    static ImGuiKey mapKey(SDL_Keycode keycode, SDL_Scancode scancode);
    static void updateKeyModifiers(ImGuiIO& io, SDL_Keymod mods);
    static bool eventBelongsToMainWindow(const SDL_Event* event);
    static bool eventWatch(void* userData, SDL_Event* event);
    static void ensureEventWatch();
    static bool ensureRenderResources();
    static void ensureFontTexture();
    static bgfx::TextureHandle resolveTexture(ImTextureID texID);
    static void setupRenderState(ImDrawData* drawData, uint16_t fbWidth, uint16_t fbHeight);
    static void renderDrawData(ImDrawData* drawData);
    static void updateMouseCursor();
    static void beginFrame(float deltaInSeconds);
    static GUIRendererImGUIState* getState();
    static bgfx::ShaderHandle createShader(ShaderBufferView shader);
    static void renderTick(float deltaInSeconds);
};

// Create ImGui context early so GUIManager::init() can safely use ImGui::GetStyle().
struct GUIRendererImGUIBootstrap {
    GUIRendererImGUIBootstrap();
};

inline GUIRendererImGUIBootstrap g_guiRendererImGUIBootstrap;

