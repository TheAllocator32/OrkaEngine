#ifndef ORKA_DISABLE_GUI
/*
 * File: GUIRendererImGUI.cpp
 *
 * Dear ImGui backend implementation for Orka's bgfx renderer.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "GUIRendererImGUI.hpp"

#include "../essentials/Logger.hpp"
#include "../primitives/renderer/Texture.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/shaders/CompiledRendererShaders.hpp"
#include "../window/Window.hpp"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <string>

static constexpr uint8_t xg_guiRendererImGUIViewID = 240;

struct GUIRendererImGUIState {
    bool initialized = false;
    bool frameStarted = false;
    bool eventWatchInstalled = false;
    bool renderResourcesReady = false;
    bool fontAtlasDirty = true;
    bool missingShaderLogged = false;
    bool transientBufferWarningLogged = false;
    bool dynamicBufferFallbackLogged = false;
    uint32_t transientBufferSkippedLists = 0;

    Identifier registeredDrawFunctionID = Identifier::INVALID;

    Texture fontTexture;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle textureUniform = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout vertexLayout;
    bool vertexLayoutReady = false;

    std::array<SDL_Cursor*, ImGuiMouseCursor_COUNT> cursors{};
    std::array<bool, 5> mouseButtonsDown{};
    float mouseWheelX = 0.0f;
    float mouseWheelY = 0.0f;
};

GUIRendererImGUIState* GUIRendererImGUI::getState() {
    static GUIRendererImGUIState* state = new GUIRendererImGUIState();
    return state;
}

const char* GUIRendererImGUI::getClipboardText(void* userData) {
    (void)userData;
    static std::string clipboard;
    char* text = SDL_GetClipboardText();
    clipboard = (text != nullptr) ? text : "";
    SDL_free(text);
    return clipboard.c_str();
}

void GUIRendererImGUI::setClipboardText(void* userData, const char* text) {
    (void)userData;
    SDL_SetClipboardText(text != nullptr ? text : "");
}

ImGuiKey GUIRendererImGUI::mapKey(SDL_Keycode keycode, SDL_Scancode scancode) {
    switch (keycode) {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        case SDLK_APOSTROPHE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        case SDLK_EQUALS: return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDLK_GRAVE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_KP_0: return ImGuiKey_Keypad0;
        case SDLK_KP_1: return ImGuiKey_Keypad1;
        case SDLK_KP_2: return ImGuiKey_Keypad2;
        case SDLK_KP_3: return ImGuiKey_Keypad3;
        case SDLK_KP_4: return ImGuiKey_Keypad4;
        case SDLK_KP_5: return ImGuiKey_Keypad5;
        case SDLK_KP_6: return ImGuiKey_Keypad6;
        case SDLK_KP_7: return ImGuiKey_Keypad7;
        case SDLK_KP_8: return ImGuiKey_Keypad8;
        case SDLK_KP_9: return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
        default:
            break;
    }

    switch (scancode) {
        case SDL_SCANCODE_A: return ImGuiKey_A;
        case SDL_SCANCODE_B: return ImGuiKey_B;
        case SDL_SCANCODE_C: return ImGuiKey_C;
        case SDL_SCANCODE_D: return ImGuiKey_D;
        case SDL_SCANCODE_E: return ImGuiKey_E;
        case SDL_SCANCODE_F: return ImGuiKey_F;
        case SDL_SCANCODE_G: return ImGuiKey_G;
        case SDL_SCANCODE_H: return ImGuiKey_H;
        case SDL_SCANCODE_I: return ImGuiKey_I;
        case SDL_SCANCODE_J: return ImGuiKey_J;
        case SDL_SCANCODE_K: return ImGuiKey_K;
        case SDL_SCANCODE_L: return ImGuiKey_L;
        case SDL_SCANCODE_M: return ImGuiKey_M;
        case SDL_SCANCODE_N: return ImGuiKey_N;
        case SDL_SCANCODE_O: return ImGuiKey_O;
        case SDL_SCANCODE_P: return ImGuiKey_P;
        case SDL_SCANCODE_Q: return ImGuiKey_Q;
        case SDL_SCANCODE_R: return ImGuiKey_R;
        case SDL_SCANCODE_S: return ImGuiKey_S;
        case SDL_SCANCODE_T: return ImGuiKey_T;
        case SDL_SCANCODE_U: return ImGuiKey_U;
        case SDL_SCANCODE_V: return ImGuiKey_V;
        case SDL_SCANCODE_W: return ImGuiKey_W;
        case SDL_SCANCODE_X: return ImGuiKey_X;
        case SDL_SCANCODE_Y: return ImGuiKey_Y;
        case SDL_SCANCODE_Z: return ImGuiKey_Z;
        default:
            break;
    }

    return ImGuiKey_None;
}

void GUIRendererImGUI::updateKeyModifiers(ImGuiIO& io, SDL_Keymod mods) {
    io.AddKeyEvent(ImGuiMod_Ctrl, (mods & SDL_KMOD_CTRL) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (mods & SDL_KMOD_SHIFT) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (mods & SDL_KMOD_ALT) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (mods & SDL_KMOD_GUI) != 0);
}

bool GUIRendererImGUI::eventBelongsToMainWindow(const SDL_Event* event) {
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
        default:
            break;
    }

    return true;
}

bool GUIRendererImGUI::eventWatch(void* userdata, SDL_Event* event) {
    (void)userdata;
    GUIRendererImGUIState* state = getState();
    if (ImGui::GetCurrentContext() == nullptr || event == nullptr) {
        return true;
    }

    if (!eventBelongsToMainWindow(event)) {
        return true;
    }

    ImGuiIO& io = ImGui::GetIO();

    switch (event->type) {
        case SDL_EVENT_MOUSE_WHEEL: {
            float wheelX = event->wheel.x;
            float wheelY = event->wheel.y;
            if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                wheelX = -wheelX;
                wheelY = -wheelY;
            }
            state->mouseWheelX += wheelX;
            state->mouseWheelY += wheelY;
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int button = -1;
            switch (event->button.button) {
                case SDL_BUTTON_LEFT: button = 0; break;
                case SDL_BUTTON_RIGHT: button = 1; break;
                case SDL_BUTTON_MIDDLE: button = 2; break;
                case SDL_BUTTON_X1: button = 3; break;
                case SDL_BUTTON_X2: button = 4; break;
                default: break;
            }
            if (button >= 0 && button < static_cast<int>(state->mouseButtonsDown.size())) {
                state->mouseButtonsDown[button] = event->button.down;
            }
            return true;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (event->text.text != nullptr) {
                io.AddInputCharactersUTF8(event->text.text);
            }
            return true;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            const ImGuiKey key = mapKey(event->key.key, event->key.scancode);
            if (key != ImGuiKey_None) {
                io.AddKeyEvent(key, event->key.down);
            }
            updateKeyModifiers(io, static_cast<SDL_Keymod>(event->key.mod));
            return true;
        }
        default:
            return true;
    }

    return true;
}

void GUIRendererImGUI::ensureEventWatch() {
    GUIRendererImGUIState* state = getState();
    if (state->eventWatchInstalled) {
        return;
    }

    if ((SDL_WasInit(SDL_INIT_EVENTS) & SDL_INIT_EVENTS) == 0) {
        return;
    }

    if (SDL_AddEventWatch(eventWatch, state)) {
        state->eventWatchInstalled = true;
    } else {
        Logger::log(LET_WARN, "Orka, GUIRendererImGUI", "Failed to install SDL event watch for ImGui input");
    }
}

bgfx::ShaderHandle GUIRendererImGUI::createShader(ShaderBufferView shader) {
    if (shader.empty()) {
        return BGFX_INVALID_HANDLE;
    }
    const bgfx::Memory* shaderMem = bgfx::copy(shader.data, shader.size);
    return bgfx::createShader(shaderMem);
}

bool GUIRendererImGUI::ensureRenderResources() {
    GUIRendererImGUIState* state = getState();

    const bgfx::Caps* caps = bgfx::getCaps();
    if (caps == nullptr) {
        return false;
    }

    if (!state->vertexLayoutReady) {
        state->vertexLayout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
        state->vertexLayoutReady = true;
    }

    if (!bgfx::isValid(state->textureUniform)) {
        state->textureUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    }

    if (!bgfx::isValid(state->program)) {
        if (GUIShader.getVertexSourceMode() == SSM_NONE || GUIShader.getFragmentSourceMode() == SSM_NONE) {
            GUIShader = Shader::fromAutogenerated<gui>();
        }

        const ShaderBufferView vertexShader = GUIShader.getVertexBuffer();
        const ShaderBufferView fragmentShader = GUIShader.getFragmentBuffer();
        if (vertexShader.empty() || fragmentShader.empty()) {
            if (!state->missingShaderLogged) {
                state->missingShaderLogged = true;
                Logger::log(
                    LET_ERROR,
                    "Orka, GUIRendererImGUI",
                    std::string("ImGui shader set unavailable for renderer '") + bgfx::getRendererName(caps->rendererType) + "'."
                );
            }
            return false;
        }

        bgfx::ShaderHandle vsh = createShader(vertexShader);
        bgfx::ShaderHandle fsh = createShader(fragmentShader);
        if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
            if (bgfx::isValid(vsh)) {
                bgfx::destroy(vsh);
            }
            if (bgfx::isValid(fsh)) {
                bgfx::destroy(fsh);
            }
            Logger::log(LET_ERROR, "Orka, GUIRendererImGUI", "Failed to create ImGui shaders");
            return false;
        }

        state->program = bgfx::createProgram(vsh, fsh, true);
        if (!bgfx::isValid(state->program)) {
            Logger::log(LET_ERROR, "Orka, GUIRendererImGUI", "Failed to create ImGui shader program");
            return false;
        }
    }

    state->renderResourcesReady = bgfx::isValid(state->program) && bgfx::isValid(state->textureUniform);
    return state->renderResourcesReady;
}

void GUIRendererImGUI::ensureFontTexture() {
    GUIRendererImGUIState* state = getState();
    ImGuiIO& io = ImGui::GetIO();

    const ImTextureID expectedTexID = reinterpret_cast<ImTextureID>(&state->fontTexture);
#if IMGUI_VERSION_NUM >= 19200
    const ImTextureID currentTexID = io.Fonts->TexRef.GetTexID();
#else
    const ImTextureID currentTexID = io.Fonts->TexID;
#endif
    const bool needsUpload = state->fontAtlasDirty || !state->fontTexture || currentTexID != expectedTexID || !io.Fonts->IsBuilt();
    if (!needsUpload) {
        return;
    }

    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    if (pixels == nullptr || width <= 0 || height <= 0) {
        Logger::log(LET_WARN, "Orka, GUIRendererImGUI", "ImGui font atlas build returned no pixels");
        return;
    }

    const bgfx::Memory* mem = bgfx::copy(pixels, static_cast<uint32_t>(width * height * 4));
    bgfx::TextureHandle fontHandle = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem
    );

    if (!bgfx::isValid(fontHandle)) {
        Logger::log(LET_ERROR, "Orka, GUIRendererImGUI", "Failed to upload ImGui font texture");
        return;
    }

    state->fontTexture = fontHandle;
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(&state->fontTexture));
    io.Fonts->ClearTexData();
    state->fontAtlasDirty = false;
}

bgfx::TextureHandle GUIRendererImGUI::resolveTexture(ImTextureID texID) {
    GUIRendererImGUIState* state = getState();

    if (texID == 0) {
        return static_cast<bgfx::TextureHandle>(state->fontTexture);
    }

    Texture* texture = reinterpret_cast<Texture*>(texID);
    if (texture != nullptr && static_cast<bool>(*texture)) {
        return static_cast<bgfx::TextureHandle>(*texture);
    }

    return static_cast<bgfx::TextureHandle>(state->fontTexture);
}

void GUIRendererImGUI::setupRenderState(ImDrawData* drawData, uint16_t fbWidth, uint16_t fbHeight) {
    const bgfx::Caps* caps = bgfx::getCaps();
    if (caps == nullptr) {
        return;
    }

    float ortho[16];
    const float left = drawData->DisplayPos.x;
    const float right = drawData->DisplayPos.x + drawData->DisplaySize.x;
    const float top = drawData->DisplayPos.y;
    const float bottom = drawData->DisplayPos.y + drawData->DisplaySize.y;
    bx::mtxOrtho(ortho, left, right, bottom, top, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);

    bgfx::setViewMode(xg_guiRendererImGUIViewID, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(xg_guiRendererImGUIViewID, 0, 0, fbWidth, fbHeight);
    bgfx::setViewTransform(xg_guiRendererImGUIViewID, nullptr, ortho);
    bgfx::touch(xg_guiRendererImGUIViewID);
}

void GUIRendererImGUI::renderDrawData(ImDrawData* drawData) {
    if (drawData == nullptr || drawData->TotalVtxCount <= 0 || drawData->TotalIdxCount <= 0) {
        return;
    }

    if (!ensureRenderResources()) {
        return;
    }

    ensureFontTexture();

    const int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    const int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0) {
        return;
    }

    GUIRendererImGUIState* state = getState();
    state->transientBufferSkippedLists = 0;

    setupRenderState(drawData, static_cast<uint16_t>(fbWidth), static_cast<uint16_t>(fbHeight));

    const ImVec2 clipOff = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;

    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
        const ImDrawList* cmdList = drawData->CmdLists[listIndex];
        if (cmdList == nullptr || cmdList->VtxBuffer.Size <= 0 || cmdList->IdxBuffer.Size <= 0) {
            continue;
        }

        const uint32_t listVtxCount = static_cast<uint32_t>(cmdList->VtxBuffer.Size);
        const uint32_t listIdxCount = static_cast<uint32_t>(cmdList->IdxBuffer.Size);

        const uint32_t availVtx = bgfx::getAvailTransientVertexBuffer(listVtxCount, state->vertexLayout);
        const uint32_t availIdx = bgfx::getAvailTransientIndexBuffer(listIdxCount, sizeof(ImDrawIdx) == 4);
        const bool useTransientBuffers = availVtx >= listVtxCount && availIdx >= listIdxCount;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        bgfx::VertexBufferHandle fallbackVbh = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle fallbackIbh = BGFX_INVALID_HANDLE;

        if (useTransientBuffers) {
            bgfx::allocTransientVertexBuffer(&tvb, listVtxCount, state->vertexLayout);
            bgfx::allocTransientIndexBuffer(&tib, listIdxCount, sizeof(ImDrawIdx) == 4);

            std::memcpy(tvb.data, cmdList->VtxBuffer.Data, static_cast<size_t>(cmdList->VtxBuffer.Size) * sizeof(ImDrawVert));
            std::memcpy(tib.data, cmdList->IdxBuffer.Data, static_cast<size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx));
        } else {
            ++state->transientBufferSkippedLists;
            if (!state->transientBufferWarningLogged) {
                state->transientBufferWarningLogged = true;
                Logger::log(LET_WARN, "Orka, GUIRendererImGUI", "ImGui draw list exceeded available transient buffer space; falling back to regular buffers");
            }

            const bgfx::Memory* vertexMem = bgfx::copy(
                cmdList->VtxBuffer.Data,
                static_cast<uint32_t>(static_cast<size_t>(cmdList->VtxBuffer.Size) * sizeof(ImDrawVert))
            );
            const bgfx::Memory* indexMem = bgfx::copy(
                cmdList->IdxBuffer.Data,
                static_cast<uint32_t>(static_cast<size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx))
            );

            fallbackVbh = bgfx::createVertexBuffer(vertexMem, state->vertexLayout);
            fallbackIbh = bgfx::createIndexBuffer(indexMem, sizeof(ImDrawIdx) == 4 ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE);

            if ((!bgfx::isValid(fallbackVbh) || !bgfx::isValid(fallbackIbh)) && !state->dynamicBufferFallbackLogged) {
                state->dynamicBufferFallbackLogged = true;
                Logger::log(LET_ERROR, "Orka, GUIRendererImGUI", "Failed to create fallback ImGui buffers");
            }

            if (!bgfx::isValid(fallbackVbh) || !bgfx::isValid(fallbackIbh)) {
                if (bgfx::isValid(fallbackVbh)) {
                    bgfx::destroy(fallbackVbh);
                }
                if (bgfx::isValid(fallbackIbh)) {
                    bgfx::destroy(fallbackIbh);
                }
                continue;
            }
        }

        for (int cmdIndex = 0; cmdIndex < cmdList->CmdBuffer.Size; ++cmdIndex) {
            const ImDrawCmd* cmd = &cmdList->CmdBuffer[cmdIndex];

            if (cmd->UserCallback != nullptr) {
                if (cmd->UserCallback == ImDrawCallback_ResetRenderState) {
                    setupRenderState(drawData, static_cast<uint16_t>(fbWidth), static_cast<uint16_t>(fbHeight));
                } else {
                    cmd->UserCallback(cmdList, cmd);
                }
                continue;
            }

            ImVec4 clipRect;
            clipRect.x = (cmd->ClipRect.x - clipOff.x) * clipScale.x;
            clipRect.y = (cmd->ClipRect.y - clipOff.y) * clipScale.y;
            clipRect.z = (cmd->ClipRect.z - clipOff.x) * clipScale.x;
            clipRect.w = (cmd->ClipRect.w - clipOff.y) * clipScale.y;

            if (clipRect.x >= static_cast<float>(fbWidth) || clipRect.y >= static_cast<float>(fbHeight) || clipRect.z <= 0.0f || clipRect.w <= 0.0f) {
                continue;
            }

            const uint16_t scissorX = static_cast<uint16_t>(std::max(0.0f, clipRect.x));
            const uint16_t scissorY = static_cast<uint16_t>(std::max(0.0f, clipRect.y));
            const uint16_t scissorW = static_cast<uint16_t>(std::min(clipRect.z, static_cast<float>(fbWidth)) - std::max(0.0f, clipRect.x));
            const uint16_t scissorH = static_cast<uint16_t>(std::min(clipRect.w, static_cast<float>(fbHeight)) - std::max(0.0f, clipRect.y));

            if (scissorW == 0 || scissorH == 0) {
                continue;
            }

            const bgfx::TextureHandle textureHandle = resolveTexture(cmd->GetTexID());
            if (!bgfx::isValid(textureHandle)) {
                continue;
            }

            bgfx::setScissor(scissorX, scissorY, scissorW, scissorH);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
                           BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
            bgfx::setTexture(0, state->textureUniform, textureHandle);
            if (useTransientBuffers) {
                bgfx::setVertexBuffer(0, &tvb, cmd->VtxOffset, static_cast<uint32_t>(cmdList->VtxBuffer.Size) - cmd->VtxOffset);
            } else {
                bgfx::setVertexBuffer(0, fallbackVbh, cmd->VtxOffset, static_cast<uint32_t>(cmdList->VtxBuffer.Size) - cmd->VtxOffset);
            }
            if (useTransientBuffers) {
                bgfx::setIndexBuffer(&tib, cmd->IdxOffset, cmd->ElemCount);
            } else {
                bgfx::setIndexBuffer(fallbackIbh, cmd->IdxOffset, cmd->ElemCount);
            }
            bgfx::submit(xg_guiRendererImGUIViewID, state->program);
        }
        if (!useTransientBuffers) {
            if (bgfx::isValid(fallbackVbh)) {
                bgfx::destroy(fallbackVbh);
            }
            if (bgfx::isValid(fallbackIbh)) {
                bgfx::destroy(fallbackIbh);
            }
        }
    }

    if (state->transientBufferSkippedLists == 0) {
        state->transientBufferWarningLogged = false;
        state->dynamicBufferFallbackLogged = false;
    }
}

void GUIRendererImGUI::updateMouseCursor() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) {
        return;
    }

    GUIRendererImGUIState* state = getState();

    if (state->cursors[ImGuiMouseCursor_Arrow] == nullptr) {
        state->cursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        state->cursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
        state->cursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
        state->cursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
        state->cursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
        state->cursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
        state->cursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
        state->cursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
        state->cursors[ImGuiMouseCursor_Wait] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
        state->cursors[ImGuiMouseCursor_Progress] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_PROGRESS);
        state->cursors[ImGuiMouseCursor_NotAllowed] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
    }

    const ImGuiMouseCursor imguiCursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor || imguiCursor == ImGuiMouseCursor_None) {
        SDL_HideCursor();
        return;
    }

    SDL_Cursor* cursor = state->cursors[imguiCursor];
    if (cursor == nullptr) {
        cursor = state->cursors[ImGuiMouseCursor_Arrow];
    }

    SDL_SetCursor(cursor);
    SDL_ShowCursor();
}

void GUIRendererImGUI::beginFrame(float deltaInSeconds) {
    SDL_Window* window = Window::getSDLWindow();
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ensureEventWatch();

    ImGuiIO& io = ImGui::GetIO();

    int windowW = 0;
    int windowH = 0;
    int displayW = 0;
    int displayH = 0;
    SDL_GetWindowSize(window, &windowW, &windowH);
    if (!SDL_GetWindowSizeInPixels(window, &displayW, &displayH)) {
        displayW = windowW;
        displayH = windowH;
    }
    if (displayW <= 0 || displayH <= 0) {
        displayW = std::max(1, windowW);
        displayH = std::max(1, windowH);
    }

    io.DisplaySize = ImVec2(static_cast<float>(windowW), static_cast<float>(windowH));
    if (windowW > 0 && windowH > 0) {
        io.DisplayFramebufferScale = ImVec2(
            static_cast<float>(displayW) / static_cast<float>(windowW),
            static_cast<float>(displayH) / static_cast<float>(windowH)
        );
    } else {
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    }

    io.DeltaTime = (deltaInSeconds > 0.0f) ? deltaInSeconds : (1.0f / 60.0f);

    updateKeyModifiers(io, SDL_GetModState());

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    const SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

    if (io.WantSetMousePos) {
        SDL_WarpMouseInWindow(window, io.MousePos.x, io.MousePos.y);
    }

    const bool hasMouseFocus = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (hasMouseFocus) {
        io.AddMousePosEvent(mouseX, mouseY);
    } else {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }

    GUIRendererImGUIState* state = getState();

    io.AddMouseButtonEvent(0, state->mouseButtonsDown[0] || ((mouseButtons & SDL_BUTTON_LMASK) != 0));
    io.AddMouseButtonEvent(1, state->mouseButtonsDown[1] || ((mouseButtons & SDL_BUTTON_RMASK) != 0));
    io.AddMouseButtonEvent(2, state->mouseButtonsDown[2] || ((mouseButtons & SDL_BUTTON_MMASK) != 0));
    io.AddMouseButtonEvent(3, state->mouseButtonsDown[3] || ((mouseButtons & SDL_BUTTON_X1MASK) != 0));
    io.AddMouseButtonEvent(4, state->mouseButtonsDown[4] || ((mouseButtons & SDL_BUTTON_X2MASK) != 0));

    io.AddMouseWheelEvent(state->mouseWheelX, state->mouseWheelY);
    state->mouseWheelX = 0.0f;
    state->mouseWheelY = 0.0f;

    if (!SDL_TextInputActive(window)) {
        SDL_StartTextInput(window);
    }

    // Keep UI view transparent over 3D content.
    bgfx::setViewRect(xg_guiRendererImGUIViewID, 0, 0, static_cast<uint16_t>(displayW), static_cast<uint16_t>(displayH));
    bgfx::setViewClear(xg_guiRendererImGUIViewID, BGFX_CLEAR_NONE, 0x00000000, 1.0f, 0);
    bgfx::touch(xg_guiRendererImGUIViewID);

    updateMouseCursor();

    ImGui::NewFrame();
    state->frameStarted = true;
}

void GUIRendererImGUI::ensureInitialized() {
    GUIRendererImGUIState* state = getState();
    if (state->initialized) {
        return;
    }

    IMGUI_CHECKVERSION();
    if (ImGui::GetCurrentContext() == nullptr) {
        ImGui::CreateContext();
    }

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "orka_bgfx";
    io.BackendPlatformName = "orka_sdl3";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.GetClipboardTextFn = getClipboardText;
    io.SetClipboardTextFn = setClipboardText;
    io.ClipboardUserData = nullptr;

    state->fontAtlasDirty = true;
    state->initialized = true;
}

void GUIRendererImGUI::shutdown() {
    GUIRendererImGUIState* state = getState();

    if (state->registeredDrawFunctionID != Identifier::INVALID) {
        Renderer::removeDrawFunction(state->registeredDrawFunctionID);
        state->registeredDrawFunctionID = Identifier::INVALID;
    }

    if (state->eventWatchInstalled) {
        SDL_RemoveEventWatch(eventWatch, state);
        state->eventWatchInstalled = false;
    }

    if (bgfx::getCaps() != nullptr) {
        if (bgfx::isValid(state->program)) {
            bgfx::destroy(state->program);
            state->program = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(state->textureUniform)) {
            bgfx::destroy(state->textureUniform);
            state->textureUniform = BGFX_INVALID_HANDLE;
        }

        state->fontTexture.destroy();
    }

    for (SDL_Cursor*& cursor : state->cursors) {
        if (cursor != nullptr) {
            SDL_DestroyCursor(cursor);
            cursor = nullptr;
        }
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        if (state->frameStarted) {
            ImGui::EndFrame();
            state->frameStarted = false;
        }
        ImGui::DestroyContext();
    }

    state->renderResourcesReady = false;
    state->vertexLayoutReady = false;
    state->initialized = false;
}

std::function<void(float)> GUIRendererImGUI::getRendererLambda() {
    ensureInitialized();
    return [](float deltaInSeconds) {
        GUIRendererImGUI::renderTick(deltaInSeconds);
    };
}

Identifier GUIRendererImGUI::registerWithRenderer(RenderQueueOrder order) {
    ensureInitialized();
    GUIRendererImGUIState* state = getState();
    if (state->registeredDrawFunctionID != Identifier::INVALID) {
        return state->registeredDrawFunctionID;
    }

    state->registeredDrawFunctionID = Renderer::addDrawFunction(getRendererLambda(), order, false);
    return state->registeredDrawFunctionID;
}

void GUIRendererImGUI::unregisterFromRenderer() {
    GUIRendererImGUIState* state = getState();
    if (state->registeredDrawFunctionID == Identifier::INVALID) {
        return;
    }

    Renderer::removeDrawFunction(state->registeredDrawFunctionID);
    state->registeredDrawFunctionID = Identifier::INVALID;
}

void GUIRendererImGUI::rebuildFontAtlas() {
    GUIRendererImGUIState* state = getState();
    state->fontAtlasDirty = true;
}

void GUIRendererImGUI::prepareFrame(float deltaInSeconds) {
    ensureInitialized();

    GUIRendererImGUIState* state = getState();
    if (state->frameStarted) {
        return;
    }

    if (bgfx::getCaps() == nullptr) {
        return;
    }

    beginFrame(deltaInSeconds);
}

bool GUIRendererImGUI::ensureSharedRenderResources() {
    ensureInitialized();
    if (bgfx::getCaps() == nullptr) {
        return false;
    }
    return ensureRenderResources();
}

bgfx::ProgramHandle GUIRendererImGUI::getSharedProgram() {
    if (!ensureSharedRenderResources()) {
        return BGFX_INVALID_HANDLE;
    }
    return getState()->program;
}

bgfx::UniformHandle GUIRendererImGUI::getSharedTextureUniform() {
    if (!ensureSharedRenderResources()) {
        return BGFX_INVALID_HANDLE;
    }
    return getState()->textureUniform;
}

const bgfx::VertexLayout& GUIRendererImGUI::getSharedVertexLayout() {
    static bgfx::VertexLayout invalidLayout;
    if (!ensureSharedRenderResources()) {
        return invalidLayout;
    }
    return getState()->vertexLayout;
}

void GUIRendererImGUI::renderTick(float deltaInSeconds) {
    ensureInitialized();

    GUIRendererImGUIState* state = getState();
    if (bgfx::getCaps() == nullptr) {
        return;
    }

    if (state->frameStarted) {
        ImGui::Render();
        renderDrawData(ImGui::GetDrawData());
        state->frameStarted = false;
    }

    beginFrame(deltaInSeconds);
}

GUIRendererImGUIBootstrap::GUIRendererImGUIBootstrap() {
    GUIRendererImGUI::ensureInitialized();
}

#endif

