/*
 * File: ImGuiTools.cpp
 *
 * Contains various usefull functions related to Dear ImGUi
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Tools.hpp"

#ifndef ORKA_DISABLE_GUI
namespace ImGuiTools {
    ImVec2 p2ToVec(float x, float y, ImVec2 customVec) {
        if (customVec.x == -1) {
            ImGuiIO& io = ImGui::GetIO();
            float screenWidth = io.DisplaySize.x;
            float screenHeight = io.DisplaySize.y;
            return ImVec2(screenWidth * (x / 100.0f), screenHeight * (y / 100.0f));
        } else {
            return ImVec2(customVec.x * (x / 100.0f), customVec.y * (y / 100.0f));
        }
    }
    ImVec2 pxToVec(float x) {
        ImGuiIO& io = ImGui::GetIO();
        float screenWidth = io.DisplaySize.x;
        return ImVec2(screenWidth * (x / 100.0f), screenWidth * (x / 100.0f));
    }
    ImVec2 pyToVec(float y) {
        ImGuiIO& io = ImGui::GetIO();
        float screenHeight = io.DisplaySize.y;
        return ImVec2(screenHeight * (y / 100.0f), screenHeight * (y / 100.0f));
    }
}
#endif

