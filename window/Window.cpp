/*
* File: Window.hpp
 *
 * Controller class for the main render window
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Window.hpp"
#include <SDL3/SDL.h>
#include "../Configurator.hpp"
#ifndef ORKA_DISABLE_INPUT
#include "../input/InputHandler.hpp"
#endif

#include <charconv>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

static int getSizeComponent(const vector2di& value, bool width) {
    return width ? static_cast<int>(value.x) : static_cast<int>(value.y);
}

static int parsePositiveInt(std::string_view text, int fallback) {
    if (text.empty()) {
        return fallback;
    }

    int value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end || value <= 0) {
        return fallback;
    }
    return value;
}

static vector2di parseConfiguredSize() {
    constexpr int defaultWidth = 1280;
    constexpr int defaultHeight = 800;

    const std::string_view cfg = OrkaConfig::WINDOWSIZE.get();
    if (cfg.empty()) {
        return vector2di{defaultWidth, defaultHeight};
    }

    std::size_t sep = cfg.find_first_of("xX");
    std::size_t sepLength = 1;
    if (sep == std::string_view::npos) {
        sep = cfg.find("×");
        sepLength = 2;
    }

    if (sep == std::string_view::npos) {
        return vector2di{defaultWidth, defaultHeight};
    }

    const std::string_view wStr = cfg.substr(0, sep);
    const std::string_view hStr = cfg.substr(sep + sepLength);

    const int width = parsePositiveInt(wStr, defaultWidth);
    const int height = parsePositiveInt(hStr, defaultHeight);
    return vector2di{width, height};
}

static std::string normalizeTitle(std::string_view title) {
    if (title.empty()) {
        return "Orka Engine Render Window";
    }
    return std::string(title);
}

#ifndef ORKA_DISABLE_INPUT
static std::unordered_map<SDL_JoystickID, SDL_Gamepad*> sg_openGamepads;

template <typename EventType, typename ListenerMap>
static void dispatchInputEvent(ListenerMap& listeners, EventType& event) {
    if (listeners.empty()) {
        return;
    }

    std::vector<Identifier> listenerIDs;
    listenerIDs.reserve(listeners.size());
    for (const auto& [id, ignored] : listeners) {
        (void)ignored;
        listenerIDs.push_back(id);
    }

    for (const Identifier id : listenerIDs) {
        auto listener = listeners.find(id);
        if (listener != listeners.end()) {
            listener->second(event);
        }
    }
}
#endif

static bool belongsToMainWindow(SDL_WindowID eventWindowID, SDL_WindowID mainWindowID) {
    return (eventWindowID == 0) || (eventWindowID == mainWindowID);
}

#ifndef ORKA_DISABLE_INPUT
static void updateMousePositionFromEvent(int x, int y) {
    InputHandler::sm_previousMousePos = InputHandler::sm_mousePos;
    InputHandler::sm_mousePos = vector2di{x, y};
}

static void emitMouseEvent(MouseEventType type, int x, int y) {
    updateMousePositionFromEvent(x, y);
    MouseEvent event {
        .eventType = type,
        .x = x,
        .y = y,
        .lastX = static_cast<int>(InputHandler::sm_previousMousePos.x),
        .lastY = static_cast<int>(InputHandler::sm_previousMousePos.y)
    };
    dispatchInputEvent(InputHandler::sm_mouseEventListeners, event);
}

static void openTrackedGamepad(SDL_JoystickID instanceID) {
    if (!SDL_IsGamepad(instanceID) || sg_openGamepads.contains(instanceID)) {
        return;
    }

    SDL_Gamepad* gamepad = SDL_OpenGamepad(instanceID);
    if (gamepad != nullptr) {
        sg_openGamepads[instanceID] = gamepad;
    }
}

static void closeTrackedGamepad(SDL_JoystickID instanceID) {
    const auto it = sg_openGamepads.find(instanceID);
    if (it == sg_openGamepads.end()) {
        return;
    }

    SDL_CloseGamepad(it->second);
    sg_openGamepads.erase(it);
}

static void openAllTrackedGamepads() {
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (!gamepads) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        openTrackedGamepad(gamepads[i]);
    }
    SDL_free(gamepads);
}

static void closeAllTrackedGamepads() {
    for (const auto& [instanceID, gamepad] : sg_openGamepads) {
        (void)instanceID;
        SDL_CloseGamepad(gamepad);
    }
    sg_openGamepads.clear();
}

static bool mapGamepadButton(const SDL_GamepadButton button, ControllerEventType& type) {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_SOUTH: type = ControllerEventType::CET_BUTTON_SOUTH; return true;
        case SDL_GAMEPAD_BUTTON_EAST: type = ControllerEventType::CET_BUTTON_EAST; return true;
        case SDL_GAMEPAD_BUTTON_WEST: type = ControllerEventType::CET_BUTTON_WEST; return true;
        case SDL_GAMEPAD_BUTTON_NORTH: type = ControllerEventType::CET_BUTTON_NORTH; return true;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: type = ControllerEventType::CET_LEFT_SHOULDER; return true;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: type = ControllerEventType::CET_RIGHT_SHOULDER; return true;
        case SDL_GAMEPAD_BUTTON_BACK: type = ControllerEventType::CET_BACK; return true;
        case SDL_GAMEPAD_BUTTON_START: type = ControllerEventType::CET_START; return true;
        case SDL_GAMEPAD_BUTTON_GUIDE: type = ControllerEventType::CET_GUIDE; return true;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK: type = ControllerEventType::CET_LEFT_STICK_PRESS; return true;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: type = ControllerEventType::CET_RIGHT_STICK_PRESS; return true;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: type = ControllerEventType::CET_DPAD_UP; return true;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: type = ControllerEventType::CET_DPAD_DOWN; return true;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: type = ControllerEventType::CET_DPAD_LEFT; return true;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: type = ControllerEventType::CET_DPAD_RIGHT; return true;
        case SDL_GAMEPAD_BUTTON_MISC1: type = ControllerEventType::CET_SHARE; return true;
        case SDL_GAMEPAD_BUTTON_TOUCHPAD: type = ControllerEventType::CET_TOUCHPAD; return true;
        default:
            return false;
    }
}

static bool mapGamepadAxis(const SDL_GamepadAxis axis, ControllerEventType& type) {
    switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: type = ControllerEventType::CET_LEFT_STICK_X; return true;
        case SDL_GAMEPAD_AXIS_LEFTY: type = ControllerEventType::CET_LEFT_STICK_Y; return true;
        case SDL_GAMEPAD_AXIS_RIGHTX: type = ControllerEventType::CET_RIGHT_STICK_X; return true;
        case SDL_GAMEPAD_AXIS_RIGHTY: type = ControllerEventType::CET_RIGHT_STICK_Y; return true;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: type = ControllerEventType::CET_LEFT_TRIGGER_AXIS; return true;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: type = ControllerEventType::CET_RIGHT_TRIGGER_AXIS; return true;
        default:
            return false;
    }
}

static void updateInputHandlerForEvent(const SDL_Event& event, SDL_WindowID mainWindowID) {
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            if (!belongsToMainWindow(event.motion.windowID, mainWindowID)) {
                return;
            }
            emitMouseEvent(MET_MOVED, static_cast<int>(event.motion.x), static_cast<int>(event.motion.y));
            return;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            if (!belongsToMainWindow(event.wheel.windowID, mainWindowID)) {
                return;
            }

            const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
            MouseEvent mouseEvent {
                .eventType = MET_WHEEL,
                .x = static_cast<int>(InputHandler::sm_mousePos.x),
                .y = static_cast<int>(InputHandler::sm_mousePos.y),
                .lastX = static_cast<int>(InputHandler::sm_previousMousePos.x),
                .lastY = static_cast<int>(InputHandler::sm_previousMousePos.y),
                .wheelX = event.wheel.x * direction,
                .wheelY = event.wheel.y * direction
            };
            dispatchInputEvent(InputHandler::sm_mouseEventListeners, mouseEvent);
            return;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (!belongsToMainWindow(event.button.windowID, mainWindowID)) {
                return;
            }

            MouseEventType type = MET_MOVED;
            bool mapped = true;

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: type = MET_BUTTON_LEFT_PRESSED; break;
                    case SDL_BUTTON_MIDDLE: type = MET_BUTTON_MIDDLE_PRESSED; break;
                    case SDL_BUTTON_RIGHT: type = MET_BUTTON_RIGHT_PRESSED; break;
                    default: mapped = false; break;
                }
            } else {
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: type = MET_BUTTON_LEFT_RELEASED; break;
                    case SDL_BUTTON_MIDDLE: type = MET_BUTTON_MIDDLE_RELEASED; break;
                    case SDL_BUTTON_RIGHT: type = MET_BUTTON_RIGHT_RELEASED; break;
                    default: mapped = false; break;
                }
            }

            if (mapped) {
                emitMouseEvent(type, static_cast<int>(event.button.x), static_cast<int>(event.button.y));
            }
            return;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (!belongsToMainWindow(event.key.windowID, mainWindowID)) {
                return;
            }

            const int key = static_cast<int>(event.key.key);
            const bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
            InputHandler::sm_keysPressed[key] = pressed;

            KeyboardEvent keyboardEvent {
                .eventType = pressed ? KET_KEY_PRESSED : KET_KEY_RELEASED,
                .key = key
            };
            dispatchInputEvent(InputHandler::sm_keyboardEventListeners, keyboardEvent);
            return;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            ControllerEventType type = ControllerEventType::CET_COUNT;
            if (!mapGamepadButton(static_cast<SDL_GamepadButton>(event.gbutton.button), type)) {
                return;
            }

            const bool pressed = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            ControllerEvent controllerEvent {
                .eventType = type,
                .controllerID = static_cast<int>(event.gbutton.which),
                .pressed = pressed,
                .value = static_cast<short>(pressed ? 32767 : 0)
            };
            dispatchInputEvent(InputHandler::sm_controllerEventListeners, controllerEvent);
            return;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            ControllerEventType type = ControllerEventType::CET_COUNT;
            if (!mapGamepadAxis(static_cast<SDL_GamepadAxis>(event.gaxis.axis), type)) {
                return;
            }

            ControllerEvent controllerEvent {
                .eventType = type,
                .controllerID = static_cast<int>(event.gaxis.which),
                .pressed = (event.gaxis.value != 0),
                .value = static_cast<short>(event.gaxis.value)
            };
            dispatchInputEvent(InputHandler::sm_controllerEventListeners, controllerEvent);
            return;
        }
        default:
            return;
    }
}
#endif

void Window::create(std::string_view title) {
    const vector2di configuredSize = parseConfiguredSize();
    const bool fullscreen = (OrkaConfig::WINDOWMODE.get() == "fullscreen");
    create(title, configuredSize, fullscreen, true, true);
}

void Window::create(std::string_view title, vector2di size, bool fullscreen, bool resizable, bool maximizable) {
#ifndef ORKA_DISABLE_INPUT
    constexpr SDL_InitFlags required = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK;
#else
    constexpr SDL_InitFlags required = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#endif
    if ((SDL_WasInit(required) & required) != required) {
        SDL_Init(required);
    }

    if (sm_window != nullptr) {
        destroy();
    }

    (void)maximizable; // STUB, TODO: add real maximizable

    const int width = getSizeComponent(size, true);
    const int height = getSizeComponent(size, false);

    SDL_WindowFlags flags = 0;
    if (resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    const std::string windowTitle = normalizeTitle(title);
    sm_window = SDL_CreateWindow(windowTitle.c_str(), width, height, flags);
    if (sm_window != nullptr) {
        sm_isOpen = true;
        SDL_SetWindowPosition(sm_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_RestoreWindow(sm_window);
        SDL_ShowWindow(sm_window);
        SDL_RaiseWindow(sm_window);
#ifndef ORKA_DISABLE_INPUT
        openAllTrackedGamepads();
#endif
    } else {
        sm_isOpen = false;
    }
}

void Window::destroy() {
    sm_isOpen = false;
#ifndef ORKA_DISABLE_INPUT
    closeAllTrackedGamepads();
#endif
    if (sm_window == nullptr) {
        return;
    }

    SDL_DestroyWindow(sm_window);
    sm_window = nullptr;
}

void Window::maximize() {
    if (sm_window != nullptr) {
        SDL_MaximizeWindow(sm_window);
    }
}

void Window::minimize() {
    if (sm_window != nullptr) {
        SDL_MinimizeWindow(sm_window);
    }
}

void Window::restore() {
    if (sm_window != nullptr) {
        SDL_RestoreWindow(sm_window);
    }
}

void Window::setFullscreen(bool fullscreen) {
    if (sm_window != nullptr) {
        SDL_SetWindowFullscreen(sm_window, fullscreen);
    }
}

void Window::update() {
    if (sm_window == nullptr || !sm_isOpen) {
        return;
    }

    const SDL_WindowID mainWindowID = SDL_GetWindowID(sm_window);
    bool closeRequested = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
#ifndef ORKA_DISABLE_INPUT
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            openTrackedGamepad(event.gdevice.which);
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            closeTrackedGamepad(event.gdevice.which);
        }
#endif

        if (event.type == SDL_EVENT_QUIT) {
            closeRequested = true;
            continue;
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            belongsToMainWindow(event.window.windowID, mainWindowID)) {
            closeRequested = true;
            continue;
        }
        if (event.type == SDL_EVENT_WINDOW_DESTROYED &&
            belongsToMainWindow(event.window.windowID, mainWindowID)) {
            closeRequested = true;
            continue;
        }

#ifndef ORKA_DISABLE_INPUT
        updateInputHandlerForEvent(event, mainWindowID);
#endif
    }

    if (closeRequested) {
        sm_isOpen = false;
        SDL_HideWindow(sm_window);
    }
}

bool Window::isOpen() {
    return sm_window != nullptr && sm_isOpen;
}

void Window::resize() {
    if (sm_window == nullptr) {
        return;
    }

    const vector2di configuredSize = parseConfiguredSize();
    SDL_SetWindowSize(sm_window, getSizeComponent(configuredSize, true), getSizeComponent(configuredSize, false));
}

