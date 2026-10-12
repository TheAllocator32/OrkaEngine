/*
 * File: InputHandler.hpp
 *
 * Contains the Input Handler used to
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
#include "../essentials/Identifier.hpp"
#include "InputHandlerTypes.hpp"
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#define f(x) std::function<void(x)> // Simplifies lambda usage

// As almost all global Orka Subsystems this is a static class!
class InputHandler {
public:
    // Adds a mouse event listener (and returns its ID! You will need the ID to delete it later!)
    inline static Identifier addMouseEventListener(f(MouseEvent& evt) callback) { Identifier id = sm_mouseEventListenerCounter++; sm_mouseEventListeners[id] = callback; return id; }
    // Adds a keyboard event listener (and returns its ID! You will need the ID to delete it later!)
    inline static Identifier addKeyboardEventListener(f(KeyboardEvent& evt) callback) { Identifier id = sm_keyboardEventListenerCounter++; sm_keyboardEventListeners[id] = callback; return id; }
    // Adds a gamepad/controller event listener (and returns its ID! You will need the ID to delete it later!)
    inline static Identifier addControllerEventListener(f(ControllerEvent& evt) callback) { Identifier id = sm_controllerEventListenerCounter++; sm_controllerEventListeners[id] = callback; return id; }

    // Remove a mouse event listener by ID
    inline static void removeMouseEventListener(Identifier id) { sm_mouseEventListeners.erase(id); }
    // Remove a keyboard event listener by ID
    inline static void removeKeyboardEventListener(Identifier id) { sm_keyboardEventListeners.erase(id); }
    // Remove a controller event listener by ID
    inline static void removeControllerEventListener(Identifier id) { sm_controllerEventListeners.erase(id); }
    // Remove all mouse event listeners
    inline static void removeAllMouseEventListeners() { sm_mouseEventListeners.clear(); }
    // Remove all keyboard event listeners
    inline static void removeAllKeyboardEventListeners() { sm_keyboardEventListeners.clear(); }
    // Remove all controller event listeners
    inline static void removeAllControllerEventListeners() { sm_controllerEventListeners.clear(); }
    // Cleans up everything!
    inline static void removeAllEventListeners() {
        removeAllMouseEventListeners();
        removeAllKeyboardEventListeners();
        removeAllControllerEventListeners();
    }

    // Checks if a key (obviously on the keyboard) is pressed (usefull for quick tests)
    inline static bool isPressed(int key) { return sm_keysPressed[key]; }
    // Returns the current/previous mouse position as vector2 (usefull for quick tests)
    inline static vector2di getMousePosition() { return sm_mousePos; }
    inline static vector2di getPreviousMousePosition() { return sm_previousMousePos; }
    // Returns the SDL instance IDs of all currently connected gamepads
    static std::vector<int> getConnectedGamepads();
    // Returns whether a gamepad with this SDL instance ID is currently connected
    static bool isGamepadConnected(int controllerID);
    // Starts low-frequency gamepad rumble
    static bool rumbleGamepadLowFrequency(int controllerID, std::uint16_t strength, std::uint32_t durationMs);
    // Starts high-frequency gamepad rumble
    static bool rumbleGamepadHighFrequency(int controllerID, std::uint16_t strength, std::uint32_t durationMs);
    // Starts both low- and high-frequency gamepad rumble with separate strengths
    static bool rumbleGamepad(int controllerID, std::uint16_t lowFrequencyStrength, std::uint16_t highFrequencyStrength, std::uint32_t durationMs);
    // Starts trigger rumble for the selected trigger(s) if supported by the device
    static bool rumbleGamepadTriggers(int controllerID, std::uint16_t strength, std::uint32_t durationMs, GamepadTriggerRumbleTarget trigger = GTR_BOTH);
    // Stops whole-gamepad rumble immediately
    static void stopGamepadRumble(int controllerID);
    // Stops trigger rumble immediately
    static void stopGamepadTriggerRumble(int controllerID);

    // Public members (Window will use them)
    inline static std::unordered_map<Identifier, f(MouseEvent&)> sm_mouseEventListeners;
    inline static std::unordered_map<Identifier, f(KeyboardEvent&)> sm_keyboardEventListeners;
    inline static std::unordered_map<Identifier, f(ControllerEvent&)> sm_controllerEventListeners;
    inline static vector2di sm_mousePos, sm_previousMousePos;
    inline static std::unordered_map<int, bool> sm_keysPressed;
private:
    inline static Identifier sm_mouseEventListenerCounter = 0, sm_keyboardEventListenerCounter = 0, sm_controllerEventListenerCounter = 0;
};

#undef f

