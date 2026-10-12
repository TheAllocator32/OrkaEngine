/*
 * File: InputHandlerTypes.hpp
 *
 * Contains the enums/structs... used by the InputHandler
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

enum MouseEventType {
    MET_MOVED, MET_WHEEL, MET_BUTTON_LEFT_PRESSED, MET_BUTTON_MIDDLE_PRESSED, MET_BUTTON_RIGHT_PRESSED, MET_BUTTON_LEFT_RELEASED, MET_BUTTON_MIDDLE_RELEASED, MET_BUTTON_RIGHT_RELEASED
};

enum KeyboardEventType {
    KET_KEY_PRESSED, KET_KEY_RELEASED
};

enum GamepadTriggerRumbleTarget {
    GTR_LEFT,
    GTR_RIGHT,
    GTR_BOTH
};

enum class ControllerEventType {
    CET_BUTTON_SOUTH,     // A (Xbox), X (PS), B (Nintendo)
    CET_BUTTON_EAST,      // B (Xbox), Circle (PS), A (Nintendo)
    CET_BUTTON_WEST,      // X (Xbox), Square (PS), Y (Nintendo)
    CET_BUTTON_NORTH,     // Y (Xbox), Triangle (PS), X (Nintendo)
    CET_LEFT_SHOULDER,
    CET_RIGHT_SHOULDER,
    CET_LEFT_TRIGGER,
    CET_RIGHT_TRIGGER,
    CET_BACK,             // Select / Back / Minus
    CET_START,            // Start / Options / Plus
    CET_GUIDE,            // Xbox / PS / Home
    CET_LEFT_STICK_PRESS,
    CET_RIGHT_STICK_PRESS,
    CET_DPAD_UP,
    CET_DPAD_DOWN,
    CET_DPAD_LEFT,
    CET_DPAD_RIGHT,
    CET_LEFT_STICK_X,     // Analog
    CET_LEFT_STICK_Y,
    CET_RIGHT_STICK_X,
    CET_RIGHT_STICK_Y,
    CET_LEFT_TRIGGER_AXIS,   // Analog, CET_LEFT_TRIGGER
    CET_RIGHT_TRIGGER_AXIS,  // Analog, CET_RIGHT_TRIGGER
    CET_TOUCHPAD,         // e.g PlayStation 4/5 Touchpad
    CET_SHARE,            // e.g Share-Button (PS4/PS5), Capture (Switch)
    CET_COUNT             // Number of all event types
};


struct KeyboardEvent {
    KeyboardEventType eventType;
    int key; // This is for example a char like 'w' 'a' 's' 'd'
};

struct MouseEvent {
    MouseEventType eventType;
    int x, y, lastX, lastY;
    float wheelX = 0.0f, wheelY = 0.0f;
};

struct ControllerEvent {
    ControllerEventType eventType;
    int controllerID = -1; // SDL gamepad/joystick instance ID
    bool pressed; // If false the button was released
    short value; // The analog value for joysticks and triggers
};

