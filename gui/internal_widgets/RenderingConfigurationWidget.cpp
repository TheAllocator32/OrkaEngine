#ifndef ORKA_DISABLE_GUI
/*
 * File: RenderingConfigurationWidget.cpp
 *
 * Contains a widget to configure rendering
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "AllWidgets.hpp"
#include "../ext_imgui_toggle/imgui_toggle_presets.h"
#include "../../Configurator.hpp"
#include "../../renderer/Renderer.hpp"
#include "../../window/Window.hpp"
#include <filesystem>
#include <string>

void RenderingConfigurationWidget::draw(float delta) {
    (void)delta;

    static constexpr const char* kRenderSystemValues[] = {"auto", "vulkan", "opengl", "opengles", "dx11", "dx12", "metal", "webgl"};
    static constexpr const char* kRenderSystemLabels[] = {"Auto", "Vulkan", "OpenGL", "OpenGL ES", "DirectX 11", "DirectX 12", "Metal", "WebGL"};
    static constexpr const char* kVsyncValues[] = {"magic", "on", "off"};
    static constexpr const char* kVsyncLabels[] = {"Magic", "On", "Off"};
    static constexpr const char* kFilteringValues[] = {"nearest", "linear", "anisotropic"};
    static constexpr const char* kFilteringLabels[] = {"Nearest", "Linear", "Anisotropic"};
    static constexpr const char* kAALevelValues[] = {"NONE", "SMAA", "MSAA", "MSAAx4", "MSAAx8", "MSAA+SMAA", "MSAAx2+SMAA", "MSAAx4+SMAA"};
    static constexpr const char* kAALevelLabels[] = {"None", "SMAA", "MSAA", "MSAA x4", "MSAA x8", "MSAA + SMAA", "MSAA x2 + SMAA", "MSAA x4 + SMAA"};
    static constexpr const char* kAOValues[] = {"NONE", "SSAO", "SSAO+"};
    static constexpr const char* kAOLables[] = {"None", "SSAO", "SSAO+"};

    const ImGuiToggleConfig compactToggle = ImGuiTogglePresets::GlowingStyle();

    ImGui::SetNextWindowSize(ImVec2(430.0f, 400.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(460.0f, 20.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Rendering Configuration", nullptr, ImGuiWindowFlags_NoCollapse)) {
        const RenderSystemType renderSystemType = Renderer::getRenderSystemType();
        const char* renderSystemLabel = "Unknown";
        switch (renderSystemType) {
            case RST_VULKAN: renderSystemLabel = "Vulkan"; break;
            case RST_OPENGL4:
            case RST_OPENGL2: renderSystemLabel = "OpenGL"; break;
            case RST_OPENGLES: renderSystemLabel = "OpenGL ES"; break;
            case RST_DIRECTX12: renderSystemLabel = "DirectX 12"; break;
            case RST_DIRECTX11: renderSystemLabel = "DirectX 11"; break;
            case RST_METAL: renderSystemLabel = "Metal"; break;
            case RST_WEBGL: renderSystemLabel = "WebGL"; break;
            case RST_AUTO: renderSystemLabel = "Auto"; break;
            case RST_NONE: renderSystemLabel = "None"; break;
            default: break;
        }
        ImGui::Text("Active renderer: %s", renderSystemLabel);
        ImGui::TextUnformatted("Backend and anti-aliasing changes apply on next engine start.");
        const std::string configPath = (std::filesystem::current_path() / std::string(OrkaConfig::CONFIG_FILENAME)).string();
        ImGui::Text("Config file: %s", configPath.c_str());
        ImGui::Separator();

        int currentIndex = 0;
        for (int i = 0; i < IM_ARRAYSIZE(kRenderSystemValues); ++i) if (OrkaConfig::RENDERSYSTEM.get() == kRenderSystemValues[i]) currentIndex = i;
        int selectedIndex = currentIndex;
        if (ImGui::Combo("Renderer Backend", &selectedIndex, kRenderSystemLabels, IM_ARRAYSIZE(kRenderSystemLabels)) && selectedIndex != currentIndex) {
            OrkaConfig::RENDERSYSTEM.set(kRenderSystemValues[selectedIndex]);
        }

        currentIndex = 0;
        for (int i = 0; i < IM_ARRAYSIZE(kVsyncValues); ++i) if (OrkaConfig::VSYNC.get() == kVsyncValues[i]) currentIndex = i;
        selectedIndex = currentIndex;
        if (ImGui::Combo("VSync", &selectedIndex, kVsyncLabels, IM_ARRAYSIZE(kVsyncLabels)) && selectedIndex != currentIndex) {
            OrkaConfig::VSYNC.set(kVsyncValues[selectedIndex]);
        }

        currentIndex = 0;
        for (int i = 0; i < IM_ARRAYSIZE(kFilteringValues); ++i) if (OrkaConfig::TEXTURE_FILTERING.get() == kFilteringValues[i]) currentIndex = i;
        selectedIndex = currentIndex;
        if (ImGui::Combo("Texture Filtering", &selectedIndex, kFilteringLabels, IM_ARRAYSIZE(kFilteringLabels)) && selectedIndex != currentIndex) {
            OrkaConfig::TEXTURE_FILTERING.set(kFilteringValues[selectedIndex]);
        }

        currentIndex = 0;
        for (int i = 0; i < IM_ARRAYSIZE(kAALevelValues); ++i) if (OrkaConfig::ANTI_ALIASING.get() == kAALevelValues[i]) currentIndex = i;
        selectedIndex = currentIndex;
        if (ImGui::Combo("Anti-Aliasing", &selectedIndex, kAALevelLabels, IM_ARRAYSIZE(kAALevelLabels)) && selectedIndex != currentIndex) {
            OrkaConfig::ANTI_ALIASING.set(kAALevelValues[selectedIndex]);
        }

        currentIndex = 0;
        for (int i = 0; i < IM_ARRAYSIZE(kAOValues); ++i) if (OrkaConfig::AMBIENT_OCCLUSION.get() == kAOValues[i]) currentIndex = i;
        selectedIndex = currentIndex;
        if (ImGui::Combo("Ambient Occlusion", &selectedIndex, kAOLables, IM_ARRAYSIZE(kAOLables)) && selectedIndex != currentIndex) {
            OrkaConfig::AMBIENT_OCCLUSION.set(kAOValues[selectedIndex]);
        }

        bool hdrEnabled = OrkaConfig::HDR_RENDERING.get() == "enable";
        if (ImGui::Toggle("HDR Rendering", &hdrEnabled, compactToggle)) {
            OrkaConfig::HDR_RENDERING.set(hdrEnabled ? "enable" : "disable");
        }

        bool fullscreen = OrkaConfig::WINDOWMODE.get() == "fullscreen";
        if (ImGui::Toggle("Fullscreen", &fullscreen, compactToggle)) {
            OrkaConfig::WINDOWMODE.set(fullscreen ? "fullscreen" : "windowed");
            Window::setFullscreen(fullscreen);
            if (!fullscreen) {
                Window::restore();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Preset: Competitive", ImVec2(160.0f, 0.0f))) {
            OrkaConfig::tunePotato();
        }

        ImGui::SameLine();
        if (ImGui::Button("Preset: Quality", ImVec2(160.0f, 0.0f))) {
            OrkaConfig::tuneDataCenter();
        }

        if (ImGui::Button("Load Config", ImVec2(160.0f, 0.0f))) {
            OrkaConfig::load();
            Window::setFullscreen(OrkaConfig::WINDOWMODE.get() == "fullscreen");
            if (OrkaConfig::WINDOWMODE.get() == "windowed") {
                Window::restore();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Save Config", ImVec2(160.0f, 0.0f))) {
            OrkaConfig::save();
        }

        if (ImGui::Button("Reset Defaults", ImVec2(160.0f, 0.0f))) {
            OrkaConfig::restoreDefault();
        }

        ImGui::Spacing();
        ImGui::Text("Configured backend: %.*s", static_cast<int>(OrkaConfig::RENDERSYSTEM.get().size()), OrkaConfig::RENDERSYSTEM.get().data());
        ImGui::Text("Configured AA: %.*s", static_cast<int>(OrkaConfig::ANTI_ALIASING.get().size()), OrkaConfig::ANTI_ALIASING.get().data());
    }

    ImGui::End();
}

#endif

