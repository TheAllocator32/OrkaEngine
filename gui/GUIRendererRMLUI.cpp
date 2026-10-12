#ifndef ORKA_DISABLE_GUI
/*
 * File: GUIRendererRMLUI.cpp
 *
 * RmlUi backend implementation for Orka's bgfx renderer.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "GUIRendererRMLUI.hpp"

#include "../essentials/Logger.hpp"
#include "../primitives/renderer/Texture.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/FileLoader.hpp"
#include "../window/Window.hpp"
#include "GUIRendererImGUI.hpp"

#include <SDL3/SDL.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr uint8_t xg_guiRendererRMLUIViewID = 241;
static constexpr const char* xg_guiRendererRMLUIContextName = "orka_rmlui";

struct GUIRendererRMLUIVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t abgr;
};

struct GUIRendererRMLUITextureEntry {
    Texture texture;
    bool flipTexCoordY = false;
};

struct GUIRendererRMLUIGeometry {
    std::vector<Rml::Vertex> vertices;
    std::vector<uint16_t> indices16;
    std::vector<uint32_t> indices32;
    bool use32BitIndices = false;
};

struct GUIRendererRMLUIState {
    bool initialized = false;
    bool rmlInitialized = false;
    bool eventWatchInstalled = false;
    bool transientBufferWarningLogged = false;
    bool missingSharedPipelineLogged = false;

    Identifier registeredDrawFunctionID = Identifier::INVALID;
    float latestDelta = 1.0f / 60.0f;
    double elapsedSeconds = 0.0;

    int contextWidth = 1;
    int contextHeight = 1;
    int windowWidth = 1;
    int windowHeight = 1;
    int framebufferWidth = 1;
    int framebufferHeight = 1;
    float framebufferScaleX = 1.0f;
    float framebufferScaleY = 1.0f;

    Rml::Context* context = nullptr;
    Rml::ElementDocument* currentDocument = nullptr;
    std::string currentDocumentPath;

    std::unordered_map<Rml::CompiledGeometryHandle, GUIRendererRMLUIGeometry> geometries;
    Rml::CompiledGeometryHandle nextGeometryHandle = 1;

    std::unordered_map<Rml::TextureHandle, GUIRendererRMLUITextureEntry> textures;
    Rml::TextureHandle nextTextureHandle = 1;
    Texture whiteTexture;

    bool scissorEnabled = false;
    Rml::Rectanglei scissorRegion = Rml::Rectanglei::FromSize(Rml::Vector2i(0, 0));
    const Rml::Matrix4f* currentTransform = nullptr;
};

static GUIRendererRMLUIState* GUIRendererRMLUIGetState();
static bool GUIRendererRMLUIEnsureEventWatch();
static bool GUIRendererRMLUIEventBelongsToMainWindow(const SDL_Event* event);
static bool GUIRendererRMLUIEventWatch(void* userdata, SDL_Event* event);
static int GUIRendererRMLUIGetKeyModifiers(SDL_Keymod mods);
static Rml::Input::KeyIdentifier GUIRendererRMLUIMapKey(SDL_Keycode keycode, SDL_Scancode scancode);
static void GUIRendererRMLUIUpdateDimensions();
static bool GUIRendererRMLUIEnsureContext();
static void GUIRendererRMLUISetupView();
static void GUIRendererRMLUIEnsureWhiteTexture();
static bool GUIRendererRMLUIShouldFlipGeneratedTextureY();
static bgfx::TextureHandle GUIRendererRMLUIResolveTextureHandle(Rml::TextureHandle textureHandle, bool* outFlipTexCoordY);
static bool GUIRendererRMLUICreateTextureFromRGBA(const uint8_t* pixels, int width, int height, Texture& outTexture);
static void GUIRendererRMLUIApplyScissor();
static GUIRendererRMLUIVertex GUIRendererRMLUIBuildVertex(const Rml::Vertex& srcVertex, Rml::Vector2f translation, bool flipTexCoordY);

class GUIRendererRMLUISystemInterface final : public Rml::SystemInterface {
public:
    double GetElapsedTime() override {
        return GUIRendererRMLUIGetState()->elapsedSeconds;
    }

    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        switch (type) {
            case Rml::Log::LT_ERROR:
            case Rml::Log::LT_ASSERT:
                Logger::log(LET_ERROR, "Orka, RmlUi", std::string(message.c_str()));
                break;
            case Rml::Log::LT_WARNING:
                Logger::log(LET_WARN, "Orka, RmlUi", std::string(message.c_str()));
                break;
            case Rml::Log::LT_INFO:
                Logger::log(LET_INFO, "Orka, RmlUi", std::string(message.c_str()));
                break;
            case Rml::Log::LT_DEBUG:
            case Rml::Log::LT_ALWAYS:
            default:
                Logger::log(LET_INFO, "Orka, RmlUi", std::string(message.c_str()));
                break;
        }
        return true;
    }

    void SetClipboardText(const Rml::String& text) override {
        SDL_SetClipboardText(text.c_str());
    }

    void GetClipboardText(Rml::String& text) override {
        char* sdlText = SDL_GetClipboardText();
        text = (sdlText != nullptr) ? sdlText : "";
        SDL_free(sdlText);
    }
};

class GUIRendererRMLUIRenderInterface final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override {
        GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
        if (vertices.empty() || indices.empty()) {
            return 0;
        }

        GUIRendererRMLUIGeometry geometry;
        geometry.vertices.assign(vertices.begin(), vertices.end());

        uint32_t maxIndex = 0;
        for (const int index : indices) {
            if (index < 0) {
                continue;
            }
            maxIndex = std::max<uint32_t>(maxIndex, static_cast<uint32_t>(index));
        }

        geometry.use32BitIndices = maxIndex > std::numeric_limits<uint16_t>::max();
        if (geometry.use32BitIndices) {
            geometry.indices32.reserve(indices.size());
            for (const int index : indices) {
                if (index < 0) {
                    geometry.indices32.push_back(0);
                } else {
                    geometry.indices32.push_back(static_cast<uint32_t>(index));
                }
            }
        } else {
            geometry.indices16.reserve(indices.size());
            for (const int index : indices) {
                if (index < 0) {
                    geometry.indices16.push_back(0);
                } else {
                    geometry.indices16.push_back(static_cast<uint16_t>(index));
                }
            }
        }

        Rml::CompiledGeometryHandle handle = state->nextGeometryHandle++;
        if (handle == 0) {
            handle = state->nextGeometryHandle++;
        }
        state->geometries[handle] = std::move(geometry);
        return handle;
    }

    void RenderGeometry(Rml::CompiledGeometryHandle geometryHandle, Rml::Vector2f translation, Rml::TextureHandle textureHandle) override {
        GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
        if (!GUIRendererImGUI::ensureSharedRenderResources()) {
            if (!state->missingSharedPipelineLogged) {
                state->missingSharedPipelineLogged = true;
                Logger::log(LET_ERROR, "Orka, GUIRendererRMLUI", "Could not access shared ImGui BGFX pipeline for RmlUi rendering.");
            }
            return;
        }

        auto geometryIt = state->geometries.find(geometryHandle);
        if (geometryIt == state->geometries.end()) {
            return;
        }

        const GUIRendererRMLUIGeometry& geometry = geometryIt->second;
        if (geometry.vertices.empty()) {
            return;
        }

        const uint32_t vertexCount = static_cast<uint32_t>(geometry.vertices.size());
        const uint32_t indexCount = geometry.use32BitIndices
            ? static_cast<uint32_t>(geometry.indices32.size())
            : static_cast<uint32_t>(geometry.indices16.size());
        if (indexCount == 0) {
            return;
        }

        const bgfx::VertexLayout& layout = GUIRendererImGUI::getSharedVertexLayout();
        if (layout.getStride() == 0) {
            return;
        }

        const uint32_t availableVertices = bgfx::getAvailTransientVertexBuffer(vertexCount, layout);
        const uint32_t availableIndices = bgfx::getAvailTransientIndexBuffer(indexCount, geometry.use32BitIndices);
        if (availableVertices < vertexCount || availableIndices < indexCount) {
            if (!state->transientBufferWarningLogged) {
                state->transientBufferWarningLogged = true;
                Logger::log(LET_WARN, "Orka, GUIRendererRMLUI", "Not enough transient buffer space for RmlUi draw data.");
            }
            return;
        }

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientVertexBuffer(&tvb, vertexCount, layout);
        bgfx::allocTransientIndexBuffer(&tib, indexCount, geometry.use32BitIndices);

        GUIRendererRMLUIVertex* dstVertices = reinterpret_cast<GUIRendererRMLUIVertex*>(tvb.data);

        if (geometry.use32BitIndices) {
            std::memcpy(tib.data, geometry.indices32.data(), geometry.indices32.size() * sizeof(uint32_t));
        } else {
            std::memcpy(tib.data, geometry.indices16.data(), geometry.indices16.size() * sizeof(uint16_t));
        }

        bool flipTexCoordY = false;
        GUIRendererRMLUIEnsureWhiteTexture();
        const bgfx::TextureHandle bgfxTexture = GUIRendererRMLUIResolveTextureHandle(textureHandle, &flipTexCoordY);
        if (!bgfx::isValid(bgfxTexture)) {
            return;
        }

        for (uint32_t i = 0; i < vertexCount; ++i) {
            dstVertices[i] = GUIRendererRMLUIBuildVertex(geometry.vertices[i], translation, flipTexCoordY);
        }

        GUIRendererRMLUIApplyScissor();

        const uint64_t stateFlags = BGFX_STATE_WRITE_RGB |
                                    BGFX_STATE_WRITE_A |
                                    BGFX_STATE_MSAA |
                                    BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

        bgfx::setState(stateFlags);
        bgfx::setTexture(0, GUIRendererImGUI::getSharedTextureUniform(), bgfxTexture);
        bgfx::setVertexBuffer(0, &tvb, 0, vertexCount);
        bgfx::setIndexBuffer(&tib, 0, indexCount);
        bgfx::submit(xg_guiRendererRMLUIViewID, GUIRendererImGUI::getSharedProgram());
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {
        GUIRendererRMLUIGetState()->geometries.erase(geometry);
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override {
        texture_dimensions = Rml::Vector2i(0, 0);
        Texture texture = FileLoader::loadImage(source, false, true);
        if (!texture) {
            return 0;
        }

        GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
        Rml::TextureHandle handle = state->nextTextureHandle++;
        if (handle == 0) {
            handle = state->nextTextureHandle++;
        }
        state->textures[handle] = GUIRendererRMLUITextureEntry{std::move(texture), false};
        const auto& texRef = state->textures[handle].texture;
        texture_dimensions = Rml::Vector2i(static_cast<int>(texRef.getWidth()), static_cast<int>(texRef.getHeight()));
        return handle;
    }

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override {
        if (source_dimensions.x <= 0 || source_dimensions.y <= 0) {
            return 0;
        }

        const size_t expectedSize = static_cast<size_t>(source_dimensions.x) *
                                    static_cast<size_t>(source_dimensions.y) * 4;
        if (source.size() < expectedSize) {
            return 0;
        }

        Texture texture;
        if (!GUIRendererRMLUICreateTextureFromRGBA(source.data(), source_dimensions.x, source_dimensions.y, texture)) {
            return 0;
        }

        GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
        Rml::TextureHandle handle = state->nextTextureHandle++;
        if (handle == 0) {
            handle = state->nextTextureHandle++;
        }
        state->textures[handle] = GUIRendererRMLUITextureEntry{std::move(texture), GUIRendererRMLUIShouldFlipGeneratedTextureY()};
        return handle;
    }

    void ReleaseTexture(Rml::TextureHandle texture) override {
        GUIRendererRMLUIGetState()->textures.erase(texture);
    }

    void EnableScissorRegion(bool enable) override {
        GUIRendererRMLUIGetState()->scissorEnabled = enable;
    }

    void SetScissorRegion(Rml::Rectanglei region) override {
        GUIRendererRMLUIGetState()->scissorRegion = region;
    }

    void SetTransform(const Rml::Matrix4f* transform) override {
        GUIRendererRMLUIGetState()->currentTransform = transform;
    }
};

static GUIRendererRMLUISystemInterface sg_guiRendererRMLUISystemInterface;
static GUIRendererRMLUIRenderInterface sg_guiRendererRMLUIRenderInterface;

static GUIRendererRMLUIState* GUIRendererRMLUIGetState() {
    static GUIRendererRMLUIState* state = new GUIRendererRMLUIState();
    return state;
}

static bool GUIRendererRMLUIEventBelongsToMainWindow(const SDL_Event* event) {
    SDL_Window* window = Window::getSDLWindow();
    if (window == nullptr) {
        return true;
    }

    const SDL_WindowID mainID = SDL_GetWindowID(window);
    if (mainID == 0) {
        return true;
    }

    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            return event->key.windowID == 0 || event->key.windowID == mainID;
        case SDL_EVENT_TEXT_INPUT:
            return event->text.windowID == 0 || event->text.windowID == mainID;
        case SDL_EVENT_MOUSE_MOTION:
            return event->motion.windowID == 0 || event->motion.windowID == mainID;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return event->button.windowID == 0 || event->button.windowID == mainID;
        case SDL_EVENT_MOUSE_WHEEL:
            return event->wheel.windowID == 0 || event->wheel.windowID == mainID;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            return event->window.windowID == 0 || event->window.windowID == mainID;
        default:
            break;
    }

    return true;
}

static int GUIRendererRMLUIGetKeyModifiers(SDL_Keymod mods) {
    int modifiers = 0;
    if (mods & SDL_KMOD_CTRL) {
        modifiers |= Rml::Input::KM_CTRL;
    }
    if (mods & SDL_KMOD_SHIFT) {
        modifiers |= Rml::Input::KM_SHIFT;
    }
    if (mods & SDL_KMOD_ALT) {
        modifiers |= Rml::Input::KM_ALT;
    }
    if (mods & SDL_KMOD_GUI) {
        modifiers |= Rml::Input::KM_META;
    }
    if (mods & SDL_KMOD_CAPS) {
        modifiers |= Rml::Input::KM_CAPSLOCK;
    }
    if (mods & SDL_KMOD_NUM) {
        modifiers |= Rml::Input::KM_NUMLOCK;
    }
    return modifiers;
}

static Rml::Input::KeyIdentifier GUIRendererRMLUIMapKey(SDL_Keycode keycode, SDL_Scancode scancode) {
    switch (keycode) {
        case SDLK_TAB: return Rml::Input::KI_TAB;
        case SDLK_LEFT: return Rml::Input::KI_LEFT;
        case SDLK_RIGHT: return Rml::Input::KI_RIGHT;
        case SDLK_UP: return Rml::Input::KI_UP;
        case SDLK_DOWN: return Rml::Input::KI_DOWN;
        case SDLK_PAGEUP: return Rml::Input::KI_PRIOR;
        case SDLK_PAGEDOWN: return Rml::Input::KI_NEXT;
        case SDLK_HOME: return Rml::Input::KI_HOME;
        case SDLK_END: return Rml::Input::KI_END;
        case SDLK_INSERT: return Rml::Input::KI_INSERT;
        case SDLK_DELETE: return Rml::Input::KI_DELETE;
        case SDLK_BACKSPACE: return Rml::Input::KI_BACK;
        case SDLK_SPACE: return Rml::Input::KI_SPACE;
        case SDLK_RETURN: return Rml::Input::KI_RETURN;
        case SDLK_ESCAPE: return Rml::Input::KI_ESCAPE;
        case SDLK_PAUSE: return Rml::Input::KI_PAUSE;
        case SDLK_CAPSLOCK: return Rml::Input::KI_CAPITAL;
        case SDLK_SCROLLLOCK: return Rml::Input::KI_SCROLL;
        case SDLK_NUMLOCKCLEAR: return Rml::Input::KI_NUMLOCK;
        case SDLK_PRINTSCREEN: return Rml::Input::KI_SNAPSHOT;
        case SDLK_F1: return Rml::Input::KI_F1;
        case SDLK_F2: return Rml::Input::KI_F2;
        case SDLK_F3: return Rml::Input::KI_F3;
        case SDLK_F4: return Rml::Input::KI_F4;
        case SDLK_F5: return Rml::Input::KI_F5;
        case SDLK_F6: return Rml::Input::KI_F6;
        case SDLK_F7: return Rml::Input::KI_F7;
        case SDLK_F8: return Rml::Input::KI_F8;
        case SDLK_F9: return Rml::Input::KI_F9;
        case SDLK_F10: return Rml::Input::KI_F10;
        case SDLK_F11: return Rml::Input::KI_F11;
        case SDLK_F12: return Rml::Input::KI_F12;
        case SDLK_LCTRL: return Rml::Input::KI_LCONTROL;
        case SDLK_LSHIFT: return Rml::Input::KI_LSHIFT;
        case SDLK_LALT: return Rml::Input::KI_LMENU;
        case SDLK_LGUI: return Rml::Input::KI_LMETA;
        case SDLK_RCTRL: return Rml::Input::KI_RCONTROL;
        case SDLK_RSHIFT: return Rml::Input::KI_RSHIFT;
        case SDLK_RALT: return Rml::Input::KI_RMENU;
        case SDLK_RGUI: return Rml::Input::KI_RMETA;
        case SDLK_0: return Rml::Input::KI_0;
        case SDLK_1: return Rml::Input::KI_1;
        case SDLK_2: return Rml::Input::KI_2;
        case SDLK_3: return Rml::Input::KI_3;
        case SDLK_4: return Rml::Input::KI_4;
        case SDLK_5: return Rml::Input::KI_5;
        case SDLK_6: return Rml::Input::KI_6;
        case SDLK_7: return Rml::Input::KI_7;
        case SDLK_8: return Rml::Input::KI_8;
        case SDLK_9: return Rml::Input::KI_9;
        case SDLK_COMMA: return Rml::Input::KI_OEM_COMMA;
        case SDLK_PERIOD: return Rml::Input::KI_OEM_PERIOD;
        case SDLK_SLASH: return Rml::Input::KI_OEM_2;
        case SDLK_SEMICOLON: return Rml::Input::KI_OEM_1;
        case SDLK_EQUALS: return Rml::Input::KI_OEM_PLUS;
        case SDLK_MINUS: return Rml::Input::KI_OEM_MINUS;
        case SDLK_LEFTBRACKET: return Rml::Input::KI_OEM_4;
        case SDLK_BACKSLASH: return Rml::Input::KI_OEM_5;
        case SDLK_RIGHTBRACKET: return Rml::Input::KI_OEM_6;
        case SDLK_APOSTROPHE: return Rml::Input::KI_OEM_7;
        case SDLK_GRAVE: return Rml::Input::KI_OEM_3;
        case SDLK_KP_0: return Rml::Input::KI_NUMPAD0;
        case SDLK_KP_1: return Rml::Input::KI_NUMPAD1;
        case SDLK_KP_2: return Rml::Input::KI_NUMPAD2;
        case SDLK_KP_3: return Rml::Input::KI_NUMPAD3;
        case SDLK_KP_4: return Rml::Input::KI_NUMPAD4;
        case SDLK_KP_5: return Rml::Input::KI_NUMPAD5;
        case SDLK_KP_6: return Rml::Input::KI_NUMPAD6;
        case SDLK_KP_7: return Rml::Input::KI_NUMPAD7;
        case SDLK_KP_8: return Rml::Input::KI_NUMPAD8;
        case SDLK_KP_9: return Rml::Input::KI_NUMPAD9;
        case SDLK_KP_PERIOD: return Rml::Input::KI_DECIMAL;
        case SDLK_KP_DIVIDE: return Rml::Input::KI_DIVIDE;
        case SDLK_KP_MULTIPLY: return Rml::Input::KI_MULTIPLY;
        case SDLK_KP_MINUS: return Rml::Input::KI_SUBTRACT;
        case SDLK_KP_PLUS: return Rml::Input::KI_ADD;
        case SDLK_KP_ENTER: return Rml::Input::KI_NUMPADENTER;
        default:
            break;
    }

    switch (scancode) {
        case SDL_SCANCODE_A: return Rml::Input::KI_A;
        case SDL_SCANCODE_B: return Rml::Input::KI_B;
        case SDL_SCANCODE_C: return Rml::Input::KI_C;
        case SDL_SCANCODE_D: return Rml::Input::KI_D;
        case SDL_SCANCODE_E: return Rml::Input::KI_E;
        case SDL_SCANCODE_F: return Rml::Input::KI_F;
        case SDL_SCANCODE_G: return Rml::Input::KI_G;
        case SDL_SCANCODE_H: return Rml::Input::KI_H;
        case SDL_SCANCODE_I: return Rml::Input::KI_I;
        case SDL_SCANCODE_J: return Rml::Input::KI_J;
        case SDL_SCANCODE_K: return Rml::Input::KI_K;
        case SDL_SCANCODE_L: return Rml::Input::KI_L;
        case SDL_SCANCODE_M: return Rml::Input::KI_M;
        case SDL_SCANCODE_N: return Rml::Input::KI_N;
        case SDL_SCANCODE_O: return Rml::Input::KI_O;
        case SDL_SCANCODE_P: return Rml::Input::KI_P;
        case SDL_SCANCODE_Q: return Rml::Input::KI_Q;
        case SDL_SCANCODE_R: return Rml::Input::KI_R;
        case SDL_SCANCODE_S: return Rml::Input::KI_S;
        case SDL_SCANCODE_T: return Rml::Input::KI_T;
        case SDL_SCANCODE_U: return Rml::Input::KI_U;
        case SDL_SCANCODE_V: return Rml::Input::KI_V;
        case SDL_SCANCODE_W: return Rml::Input::KI_W;
        case SDL_SCANCODE_X: return Rml::Input::KI_X;
        case SDL_SCANCODE_Y: return Rml::Input::KI_Y;
        case SDL_SCANCODE_Z: return Rml::Input::KI_Z;
        default:
            break;
    }

    return Rml::Input::KI_UNKNOWN;
}

static bool GUIRendererRMLUIEventWatch(void* userdata, SDL_Event* event) {
    (void)userdata;
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (event == nullptr || state->context == nullptr) {
        return true;
    }

    if (!GUIRendererRMLUIEventBelongsToMainWindow(event)) {
        return true;
    }

    const int modifiers = GUIRendererRMLUIGetKeyModifiers(SDL_GetModState());

    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION: {
            const float sx = (state->windowWidth > 0)
                ? static_cast<float>(state->contextWidth) / static_cast<float>(state->windowWidth)
                : 1.0f;
            const float sy = (state->windowHeight > 0)
                ? static_cast<float>(state->contextHeight) / static_cast<float>(state->windowHeight)
                : 1.0f;
            const int mappedX = static_cast<int>(std::round(event->motion.x * sx));
            const int mappedY = static_cast<int>(std::round(event->motion.y * sy));
            state->context->ProcessMouseMove(mappedX, mappedY, modifiers);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            int buttonIndex = -1;
            switch (event->button.button) {
                case SDL_BUTTON_LEFT: buttonIndex = 0; break;
                case SDL_BUTTON_RIGHT: buttonIndex = 1; break;
                case SDL_BUTTON_MIDDLE: buttonIndex = 2; break;
                default: break;
            }
            if (buttonIndex >= 0) {
                state->context->ProcessMouseButtonDown(buttonIndex, modifiers);
            }
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int buttonIndex = -1;
            switch (event->button.button) {
                case SDL_BUTTON_LEFT: buttonIndex = 0; break;
                case SDL_BUTTON_RIGHT: buttonIndex = 1; break;
                case SDL_BUTTON_MIDDLE: buttonIndex = 2; break;
                default: break;
            }
            if (buttonIndex >= 0) {
                state->context->ProcessMouseButtonUp(buttonIndex, modifiers);
            }
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            float wheelX = event->wheel.x;
            float wheelY = event->wheel.y;
            if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                wheelX = -wheelX;
                wheelY = -wheelY;
            }
            state->context->ProcessMouseWheel(Rml::Vector2f(wheelX, -wheelY), modifiers);
            return true;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (event->text.text != nullptr) {
                state->context->ProcessTextInput(event->text.text);
            }
            return true;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            const Rml::Input::KeyIdentifier key = GUIRendererRMLUIMapKey(event->key.key, event->key.scancode);
            if (key != Rml::Input::KI_UNKNOWN) {
                if (event->type == SDL_EVENT_KEY_DOWN) {
                    state->context->ProcessKeyDown(key, modifiers);
                } else {
                    state->context->ProcessKeyUp(key, modifiers);
                }
            }
            return true;
        }
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            state->context->ProcessMouseLeave();
            return true;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            GUIRendererRMLUIUpdateDimensions();
            return true;
        default:
            return true;
    }
}

static bool GUIRendererRMLUIEnsureEventWatch() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (state->eventWatchInstalled) {
        return true;
    }

    if ((SDL_WasInit(SDL_INIT_EVENTS) & SDL_INIT_EVENTS) == 0) {
        return false;
    }

    if (SDL_AddEventWatch(GUIRendererRMLUIEventWatch, state)) {
        state->eventWatchInstalled = true;
        return true;
    }

    Logger::log(LET_WARN, "Orka, GUIRendererRMLUI", "Failed to install SDL event watch for RmlUi input.");
    return false;
}

static void GUIRendererRMLUIUpdateDimensions() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    SDL_Window* window = Window::getSDLWindow();
    if (window == nullptr) {
        return;
    }

    int windowW = 1;
    int windowH = 1;
    SDL_GetWindowSize(window, &windowW, &windowH);
    if (windowW <= 0) {
        windowW = 1;
    }
    if (windowH <= 0) {
        windowH = 1;
    }

    int displayW = windowW;
    int displayH = windowH;
    if (!SDL_GetWindowSizeInPixels(window, &displayW, &displayH)) {
        displayW = windowW;
        displayH = windowH;
    }
    if (displayW <= 0) {
        displayW = windowW;
    }
    if (displayH <= 0) {
        displayH = windowH;
    }

    state->windowWidth = windowW;
    state->windowHeight = windowH;
    state->contextWidth = windowW;
    state->contextHeight = windowH;
    state->framebufferWidth = displayW;
    state->framebufferHeight = displayH;
    state->framebufferScaleX = (windowW > 0) ? static_cast<float>(displayW) / static_cast<float>(windowW) : 1.0f;
    state->framebufferScaleY = (windowH > 0) ? static_cast<float>(displayH) / static_cast<float>(windowH) : 1.0f;

    if (state->context != nullptr) {
        state->context->SetDimensions(Rml::Vector2i(state->contextWidth, state->contextHeight));
        float displayScale = SDL_GetWindowDisplayScale(window);
        if (displayScale <= 0.0f) {
            displayScale = 1.0f;
        }
        state->context->SetDensityIndependentPixelRatio(displayScale);
    }
}

static bool GUIRendererRMLUIEnsureContext() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (state->context != nullptr) {
        GUIRendererRMLUIUpdateDimensions();
        return true;
    }

    SDL_Window* window = Window::getSDLWindow();
    if (window == nullptr) {
        return false;
    }

    GUIRendererRMLUIUpdateDimensions();
    state->context = Rml::GetContext(xg_guiRendererRMLUIContextName);
    if (state->context == nullptr) {
        state->context = Rml::CreateContext(
            xg_guiRendererRMLUIContextName,
            Rml::Vector2i(state->contextWidth, state->contextHeight)
        );
    }

    if (state->context == nullptr) {
        Logger::log(LET_ERROR, "Orka, GUIRendererRMLUI", "Failed to create RmlUi context.");
        return false;
    }

    state->context->EnableMouseCursor(true);
    float displayScale = SDL_GetWindowDisplayScale(window);
    if (displayScale <= 0.0f) {
        displayScale = 1.0f;
    }
    state->context->SetDensityIndependentPixelRatio(displayScale);
    GUIRendererRMLUIEnsureEventWatch();
    return true;
}

static void GUIRendererRMLUISetupView() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();

    float ortho[16];
    bx::mtxOrtho(
        ortho,
        0.0f,
        static_cast<float>(state->contextWidth),
        static_cast<float>(state->contextHeight),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth
    );

    bgfx::setViewMode(xg_guiRendererRMLUIViewID, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(
        xg_guiRendererRMLUIViewID,
        0,
        0,
        static_cast<uint16_t>(state->framebufferWidth),
        static_cast<uint16_t>(state->framebufferHeight)
    );
    bgfx::setViewClear(xg_guiRendererRMLUIViewID, BGFX_CLEAR_NONE, 0x00000000, 1.0f, 0);
    bgfx::setViewTransform(xg_guiRendererRMLUIViewID, nullptr, ortho);
    bgfx::touch(xg_guiRendererRMLUIViewID);
}

static bool GUIRendererRMLUICreateTextureFromRGBA(const uint8_t* pixels, int width, int height, Texture& outTexture) {
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    const size_t sizeBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    const bgfx::Memory* mem = bgfx::copy(pixels, static_cast<uint32_t>(sizeBytes));
    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        mem
    );

    if (!bgfx::isValid(handle)) {
        return false;
    }

    outTexture = handle;
    return true;
}

static void GUIRendererRMLUIEnsureWhiteTexture() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (state->whiteTexture) {
        return;
    }

    const uint8_t whitePixel[4] = {255, 255, 255, 255};
    Texture texture;
    if (GUIRendererRMLUICreateTextureFromRGBA(whitePixel, 1, 1, texture)) {
        state->whiteTexture = std::move(texture);
    }
}

static bool GUIRendererRMLUIShouldFlipGeneratedTextureY() {
    return false;
}

static bgfx::TextureHandle GUIRendererRMLUIResolveTextureHandle(Rml::TextureHandle textureHandle, bool* outFlipTexCoordY) {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (outFlipTexCoordY != nullptr) {
        *outFlipTexCoordY = false;
    }

    if (textureHandle != 0) {
        auto it = state->textures.find(textureHandle);
        if (it != state->textures.end() && static_cast<bool>(it->second.texture)) {
            if (outFlipTexCoordY != nullptr) {
                *outFlipTexCoordY = it->second.flipTexCoordY;
            }
            return static_cast<bgfx::TextureHandle>(it->second.texture);
        }
    }

    if (state->whiteTexture) {
        return static_cast<bgfx::TextureHandle>(state->whiteTexture);
    }

    return BGFX_INVALID_HANDLE;
}

static void GUIRendererRMLUIApplyScissor() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (!state->scissorEnabled) {
        bgfx::setScissor(
            0,
            0,
            static_cast<uint16_t>(std::clamp(state->framebufferWidth, 0, 65535)),
            static_cast<uint16_t>(std::clamp(state->framebufferHeight, 0, 65535))
        );
        return;
    }

    const int scaledX = static_cast<int>(std::floor(state->scissorRegion.Left() * state->framebufferScaleX));
    const int scaledY = static_cast<int>(std::floor(state->scissorRegion.Top() * state->framebufferScaleY));
    const int scaledW = static_cast<int>(std::ceil(state->scissorRegion.Width() * state->framebufferScaleX));
    const int scaledH = static_cast<int>(std::ceil(state->scissorRegion.Height() * state->framebufferScaleY));

    const int clampedX = std::clamp(scaledX, 0, state->framebufferWidth);
    const int clampedY = std::clamp(scaledY, 0, state->framebufferHeight);
    const int clampedW = std::clamp(scaledW, 0, state->framebufferWidth - clampedX);
    const int clampedH = std::clamp(scaledH, 0, state->framebufferHeight - clampedY);

    if (clampedW <= 0 || clampedH <= 0) {
        bgfx::setScissor(0, 0, 0, 0);
        return;
    }

    bgfx::setScissor(
        static_cast<uint16_t>(clampedX),
        static_cast<uint16_t>(clampedY),
        static_cast<uint16_t>(clampedW),
        static_cast<uint16_t>(clampedH)
    );
}

static GUIRendererRMLUIVertex GUIRendererRMLUIBuildVertex(const Rml::Vertex& srcVertex, Rml::Vector2f translation, bool flipTexCoordY) {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();

    float x = srcVertex.position.x + translation.x;
    float y = srcVertex.position.y + translation.y;

    if (state->currentTransform != nullptr) {
        const Rml::Vector4f transformed = (*state->currentTransform) * Rml::Vector4f(x, y, 0.0f, 1.0f);
        if (std::fabs(transformed.w) > 1e-6f) {
            x = transformed.x / transformed.w;
            y = transformed.y / transformed.w;
        } else {
            x = transformed.x;
            y = transformed.y;
        }
    }

    GUIRendererRMLUIVertex outVertex{};
    outVertex.x = x;
    outVertex.y = y;
    outVertex.u = srcVertex.tex_coord.x;
    (void)flipTexCoordY;
    outVertex.v = srcVertex.tex_coord.y;
    outVertex.abgr = static_cast<uint32_t>(srcVertex.colour.red) |
                     (static_cast<uint32_t>(srcVertex.colour.green) << 8u) |
                     (static_cast<uint32_t>(srcVertex.colour.blue) << 16u) |
                     (static_cast<uint32_t>(srcVertex.colour.alpha) << 24u);
    return outVertex;
}

void GUIRendererRMLUI::ensureInitialized() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (state->initialized) {
        return;
    }

    if (!state->rmlInitialized) {
        Rml::SetSystemInterface(&sg_guiRendererRMLUISystemInterface);
        Rml::SetRenderInterface(&sg_guiRendererRMLUIRenderInterface);
        if (!Rml::Initialise()) {
            Logger::log(LET_ERROR, "Orka, GUIRendererRMLUI", "RmlUi initialization failed.");
            return;
        }
        state->rmlInitialized = true;
    }

    GUIRendererRMLUIEnsureContext();
    GUIRendererRMLUIEnsureEventWatch();
    state->initialized = true;
}

void GUIRendererRMLUI::shutdown() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();

    if (state->registeredDrawFunctionID != Identifier::INVALID) {
        Renderer::removeDrawFunction(state->registeredDrawFunctionID);
        state->registeredDrawFunctionID = Identifier::INVALID;
    }

    if (state->eventWatchInstalled) {
        SDL_RemoveEventWatch(GUIRendererRMLUIEventWatch, state);
        state->eventWatchInstalled = false;
    }

    state->geometries.clear();
    state->textures.clear();
    state->whiteTexture.destroy();
    state->currentDocument = nullptr;
    state->currentDocumentPath.clear();

    if (state->rmlInitialized) {
        if (state->context != nullptr) {
            Rml::RemoveContext(xg_guiRendererRMLUIContextName);
            state->context = nullptr;
        }
        Rml::Shutdown();
        state->rmlInitialized = false;
    }

    state->initialized = false;
}

std::function<void(float)> GUIRendererRMLUI::getRendererLambda() {
    ensureInitialized();
    return [](float deltaInSeconds) {
        GUIRendererRMLUI::renderTick(deltaInSeconds);
    };
}

Identifier GUIRendererRMLUI::registerWithRenderer(RenderQueueOrder order) {
    ensureInitialized();
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (state->registeredDrawFunctionID != Identifier::INVALID) {
        return state->registeredDrawFunctionID;
    }

    state->registeredDrawFunctionID = Renderer::addDrawFunction(getRendererLambda(), order, false);
    return state->registeredDrawFunctionID;
}

void GUIRendererRMLUI::unregisterFromRenderer() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (state->registeredDrawFunctionID == Identifier::INVALID) {
        return;
    }

    Renderer::removeDrawFunction(state->registeredDrawFunctionID);
    state->registeredDrawFunctionID = Identifier::INVALID;
}

void GUIRendererRMLUI::prepareFrame(float deltaInSeconds) {
    ensureInitialized();
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    state->latestDelta = (deltaInSeconds > 0.0f) ? deltaInSeconds : (1.0f / 60.0f);
    GUIRendererRMLUIEnsureContext();
}

bool GUIRendererRMLUI::loadFontFace(std::string_view fontPath, bool fallbackFace) {
    ensureInitialized();
    if (fontPath.empty()) {
        return false;
    }
    const std::string fontPathString(fontPath);

    if (!Rml::LoadFontFace(fontPathString, fallbackFace)) {
        Logger::log(LET_WARN, "Orka, GUIRendererRMLUI", "Failed to load RmlUi font '" + fontPathString + "'.");
        return false;
    }
    return true;
}

bool GUIRendererRMLUI::setCurrentDocument(std::string_view documentPath) {
    ensureInitialized();
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (!GUIRendererRMLUIEnsureContext()) {
        return false;
    }

    if (state->currentDocument != nullptr) {
        state->currentDocument->Close();
        state->currentDocument = nullptr;
    }
    state->currentDocumentPath.clear();

    if (documentPath.empty()) {
        return true;
    }
    const std::string documentPathString(documentPath);

    Rml::ElementDocument* document = state->context->LoadDocument(documentPathString);
    if (document == nullptr) {
        Logger::log(LET_WARN, "Orka, GUIRendererRMLUI", "Failed to load RmlUi document '" + documentPathString + "'.");
        return false;
    }

    document->Show();
    state->currentDocument = document;
    state->currentDocumentPath = documentPathString;
    return true;
}

void GUIRendererRMLUI::clearCurrentDocument() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    if (state->currentDocument != nullptr) {
        state->currentDocument->Close();
        state->currentDocument = nullptr;
    }
    state->currentDocumentPath.clear();
}

Rml::Context* GUIRendererRMLUI::getContext() {
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();
    return state->context;
}

Rml::ElementDocument* GUIRendererRMLUI::getCurrentDocument() {
    return GUIRendererRMLUIGetState()->currentDocument;
}

void GUIRendererRMLUI::renderTick(float deltaInSeconds) {
    ensureInitialized();
    GUIRendererRMLUIState* state = GUIRendererRMLUIGetState();

    if (bgfx::getCaps() == nullptr) {
        return;
    }

    if (!GUIRendererRMLUIEnsureContext()) {
        return;
    }

    const float dt = (deltaInSeconds > 0.0f) ? deltaInSeconds : state->latestDelta;
    state->elapsedSeconds += static_cast<double>(dt);

    GUIRendererRMLUIUpdateDimensions();
    GUIRendererRMLUISetupView();

    state->currentTransform = nullptr;
    state->scissorEnabled = false;
    state->context->Update();
    state->context->Render();
}

#endif

