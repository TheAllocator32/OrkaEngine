/*
 * File: InputHandler.cpp
 *
 * InputHandler implementation for non-trivial SDL-backed helpers.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "InputHandler.hpp"
#include <SDL3/SDL.h>

static SDL_Gamepad* getOpenedGamepadByID(int controllerID) {
    return SDL_GetGamepadFromID(static_cast<SDL_JoystickID>(controllerID));
}

std::vector<int> InputHandler::getConnectedGamepads() {
    std::vector<int> result;

    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (!gamepads) return result;

    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(static_cast<int>(gamepads[i]));
    }
    SDL_free(gamepads);
    return result;
}

bool InputHandler::isGamepadConnected(int controllerID) {
    return SDL_IsGamepad(static_cast<SDL_JoystickID>(controllerID));
}

bool InputHandler::rumbleGamepadLowFrequency(int controllerID, std::uint16_t strength, std::uint32_t durationMs) {
    return rumbleGamepad(controllerID, strength, 0, durationMs);
}

bool InputHandler::rumbleGamepadHighFrequency(int controllerID, std::uint16_t strength, std::uint32_t durationMs) {
    return rumbleGamepad(controllerID, 0, strength, durationMs);
}

bool InputHandler::rumbleGamepad(int controllerID, std::uint16_t lowFrequencyStrength, std::uint16_t highFrequencyStrength, std::uint32_t durationMs) {
    SDL_Gamepad* gamepad = getOpenedGamepadByID(controllerID);
    if (!gamepad) return false;
    return SDL_RumbleGamepad(gamepad, lowFrequencyStrength, highFrequencyStrength, durationMs);
}

bool InputHandler::rumbleGamepadTriggers(int controllerID, std::uint16_t strength, std::uint32_t durationMs, GamepadTriggerRumbleTarget trigger) {
    SDL_Gamepad* gamepad = getOpenedGamepadByID(controllerID);
    if (!gamepad) return false;

    std::uint16_t leftRumble = 0;
    std::uint16_t rightRumble = 0;
    switch (trigger) {
        case GTR_LEFT:
            leftRumble = strength;
            break;
        case GTR_RIGHT:
            rightRumble = strength;
            break;
        case GTR_BOTH:
            leftRumble = strength;
            rightRumble = strength;
            break;
    }

    return SDL_RumbleGamepadTriggers(gamepad, leftRumble, rightRumble, durationMs);
}

void InputHandler::stopGamepadRumble(int controllerID) {
    (void)rumbleGamepad(controllerID, 0, 0, 0);
}

void InputHandler::stopGamepadTriggerRumble(int controllerID) {
    (void)rumbleGamepadTriggers(controllerID, 0, 0, GTR_BOTH);
}

