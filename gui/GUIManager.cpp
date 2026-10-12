#ifndef ORKA_DISABLE_GUI
/*
 * File: GUIManager.cpp
 *
 * Contains the implementation of the GUIManager class that controls GUIPages/Widgets
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "GUIManager.hpp"
#include <imgui.h>
#include "../essentials/Logger.hpp"
#include "../renderer/FileLoader.hpp"
#include "../utilities/Tools.hpp"
#include <algorithm>

#include "GUIRendererImGUI.hpp"
#include "GUIRendererRMLUI.hpp"

void GUIManager::init() { 
    GUIRendererImGUI::ensureInitialized();
    GUIRendererRMLUI::ensureInitialized();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);

    colors[ImGuiCol_Border] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.90f, 0.90f, 0.90f, 0.00f);

    colors[ImGuiCol_Header] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);

    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 0.40f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.80f, 0.30f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 1.00f, 0.40f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 0.80f, 0.30f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

    colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 1.00f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 0.80f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.60f, 0.20f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 0.40f, 1.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);

    style.FrameRounding = 7.0f;
    style.GrabRounding = 7.0f;
    style.WindowRounding = 9.0f;
    style.ScrollbarRounding = 8.0f;

    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(5, 5);
    style.ItemSpacing = ImVec2(10, 10);

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = NULL;

    GUIRendererImGUI::registerWithRenderer(RQO_UISPACE);
    GUIRendererRMLUI::registerWithRenderer(RQO_UISPACE);
}

void GUIManager::rebuildFonts() {
    ImGui::GetIO().Fonts->Build();
    GUIRendererImGUI::rebuildFontAtlas();
}

void GUIManager::unloadCurrentGUIPage() {
    if (!sm_currentGUIPage) {
        return;
    }

    sm_currentGUIPage->unload();
    sm_currentGUIPage.reset();
}

bool GUIManager::loadRMLGUIPage(std::unique_ptr<RMLGUIPage> page, std::string_view documentPath) {
    if (!page || !GUIRendererRMLUI::setCurrentDocument(documentPath)) return false;

    Rml::ElementDocument* document = GUIRendererRMLUI::getCurrentDocument();
    if (document == nullptr) {
        GUIRendererRMLUI::clearCurrentDocument();
        return false;
    }

    sm_currentRMLGUIPage = std::move(page);
    sm_currentRMLGUIPage->load(document);
    return true;
}

void GUIManager::unloadCurrentRMLGUIPage() {
    if (!sm_currentRMLGUIPage) return;

    Rml::ElementDocument* document = GUIRendererRMLUI::getCurrentDocument();
    sm_currentRMLGUIPage->unload(document);
    sm_currentRMLGUIPage->detachEventCallbacks();
    GUIRendererRMLUI::clearCurrentDocument();
    sm_currentRMLGUIPage.reset();
}

Identifier GUIManager::addWidget(std::unique_ptr<GUIWidget> widget) {
    if (!widget) return Identifier::INVALID;
    widget->load();
    Identifier id = sm_widgetCount++;
    sm_widgets[id] = std::move(widget);
    return id;
}

void GUIManager::removeWidget(Identifier id) {
    if (sm_isDrawingWidgets) {
        if (std::find(sm_pendingWidgetRemovals.begin(), sm_pendingWidgetRemovals.end(), id) == sm_pendingWidgetRemovals.end()) {
            sm_pendingWidgetRemovals.push_back(id);
        }
        return;
    }

    auto it = sm_widgets.find(id);
    if (it != sm_widgets.end()) {
        it->second->unload();
        sm_widgets.erase(it);
    }
}

GUIWidget& GUIManager::getWidget(Identifier id) {
    auto it = sm_widgets.find(id);
    if (it != sm_widgets.end()) {
        return *it->second;
    }
    throw std::runtime_error("Whoa! Widget with id " + std::to_string(id.getValue()) + " does not exist!");
}

ImFont* GUIManager::getFont(std::string_view name) {
    const auto it = sm_fonts.find(std::string(name));
    if (it == sm_fonts.end()) {
        return nullptr;
    }
    return it->second;
}

bool GUIManager::loadFont(std::string_view path, std::string_view name, float size, bool rebuild) {
    if (name.empty()) {
        Logger::log(LET_WARN, "Orka, GUIManager", "Could not load font with empty name.");
        return false;
    }
    if (size <= 0.0f) {
        Logger::log(LET_WARN, "Orka, GUIManager", "Could not load font '" + std::string(name) + "' with invalid size.");
        return false;
    }
    const std::string nameString(name);
    const auto existing = sm_fonts.find(nameString);
    if (existing != sm_fonts.end() && existing->second != nullptr) {
        if (rebuild) {
            rebuildFonts();
        }
        return true;
    }

    const std::string pathString(path);
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(pathString.c_str(), size);
    if (font == nullptr) {
        Logger::log(LET_WARN, "Orka, GUIManager", "Could not load font '" + pathString + "'.");
        return false;
    }

    sm_fonts[nameString] = font;
    if (rebuild) {
        rebuildFonts();
    }
    return true;
}

bool GUIManager::setFont(std::string_view name) {
    const auto it = sm_fonts.find(std::string(name));
    if (it == sm_fonts.end() || it->second == nullptr) {
        Logger::log(LET_WARN, "Orka, GUIManager", "Could not set unknown font '" + std::string(name) + "'.");
        return false;
    }

    ImGui::GetIO().FontDefault = it->second;
    return true;
}

void GUIManager::loadImage(std::string_view path, std::string_view name) {
    const std::string pathString(path);
    const std::string nameString(name);
    Logger::log(LET_INFO, "Orka, GUIManager", "Loading image '" + pathString + "'");
    Texture tex = FileLoader::loadImage(path, false); // We don't need MipMaps for GUI
    if (!tex) {
        Logger::log(LET_WARN, "Orka, GUIManager", "Oof... Image '" + pathString + "' could not be loaded! This will destroy GUI parts!");
    }
    sm_images[nameString] = std::move(tex); // Now GUIManager OWNS the texture
}

ImTextureID GUIManager::getImage(std::string_view name) { return reinterpret_cast<ImTextureID>(&sm_images[std::string(name)]); }

bool GUIManager::removeImage(std::string_view name) {
    auto itMap = sm_images.find(std::string(name));
    if (itMap != sm_images.end()) {
        auto& image = itMap->second;
        image.destroy();
        sm_images.erase(itMap);
        return true;
    }
    return false;
}

void GUIManager::update(float delta) {
    GUIRendererImGUI::prepareFrame(delta);
    GUIRendererRMLUI::prepareFrame(delta);
    if (sm_currentGUIPage) {
        sm_currentGUIPage->draw(delta);
    }
    if (sm_currentRMLGUIPage) {
        if (Rml::ElementDocument* document = GUIRendererRMLUI::getCurrentDocument()) {
            sm_currentRMLGUIPage->update(delta, document);
        }
    }
    std::vector<Identifier> widgetIds;
    widgetIds.reserve(sm_widgets.size());
    for (const auto& [id, widget] : sm_widgets) {
        (void)widget;
        widgetIds.push_back(id);
    }

    sm_isDrawingWidgets = true;
    for (const Identifier id : widgetIds) {
        const auto it = sm_widgets.find(id);
        if (it != sm_widgets.end() && it->second) {
            it->second->draw(delta);
        }
    }
    sm_isDrawingWidgets = false;

    const std::vector<Identifier> pendingRemovals = std::move(sm_pendingWidgetRemovals);
    sm_pendingWidgetRemovals.clear();
    for (const Identifier id : pendingRemovals) {
        removeWidget(id);
    }
}

bool GUIManager::loadRmlFont(std::string_view path, bool fallbackFace) {
    return GUIRendererRMLUI::loadFontFace(path, fallbackFace);
}

Rml::Context* GUIManager::getRmlContext() {
    return GUIRendererRMLUI::getContext();
}

Rml::ElementDocument* GUIManager::getCurrentRmlPage() {
    return GUIRendererRMLUI::getCurrentDocument();
}

#endif

