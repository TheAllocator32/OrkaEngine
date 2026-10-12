/*
 * File: GUIManager.hpp
 *
 * This file contains the GUIManager. The GUIManager controlls the visibility of GUI "Pages" and "Widgets" (defined in GUITypes.hpp)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../primitives/gui/GUITypes.hpp"
#include <imgui.h>
#include <string>
#include <memory>
#include <vector>
#include <type_traits>
#include <unordered_map>
#include "Translator.hpp"
#include "../primitives/renderer/Texture.hpp"
#include "ext_imgui_toggle/imgui_toggle.h"
#include "ext_imspinner.h"

namespace Rml { // Forward decl
    class Context;
    class ElementDocument;
}

// The GUIManager controlls, initializes and updates GUIPages and Widgets AND manages images and fonts
class GUIManager {
public:
    // Well... I think it's obvious what this function does ;)
    static void init();

    // All it does is in its name..... It also deinitialises the old GUIPage before setting the new one!
    template <GUIPageSubClass PageType>
    inline static void setPage() { unloadCurrentGUIPage(); sm_currentGUIPage = std::make_unique<PageType>(); sm_currentGUIPage->load(); }
    template <StaticGUIPageConcept StaticGUIPage>
    inline static void setStaticGUIPage(bool loadOnStart = false, bool unloadOnExit = false) { unloadCurrentGUIPage(); sm_currentGUIPage = std::make_unique<StaticGUIPageWrapper<StaticGUIPage>>(loadOnStart, unloadOnExit); sm_currentGUIPage->load(); }
    static void unloadCurrentGUIPage();

    // Loads and owns an RmlUi page declared with RMLGUIPage::RML_FILE.
    template <RMLGUIPageSubClass PageType>
    inline static bool setRMLGUIPage() { unloadCurrentRMLGUIPage(); return loadRMLGUIPage(std::make_unique<PageType>(), PageType::RML_FILE); }
    template <StaticRMLGUIPageConcept StaticRMLGUIPage>
    inline static bool setStaticRMLGUIPage(bool loadOnStart = false, bool unloadOnExit = false) { unloadCurrentRMLGUIPage(); return loadRMLGUIPage(std::make_unique<StaticRMLGUIPageWrapper<StaticRMLGUIPage>>(loadOnStart, unloadOnExit), StaticRMLGUIPage::RML_FILE); }
    static void unloadCurrentRMLGUIPage();

    // Yep, this function adds a GUIWidget to the Render Queue. It returns the ID of the widget that can later be used to remove it (see "removeWidget")
    template <GUIWidgetSubClass PageType>
    inline static Identifier addWidget() { return addWidget(std::make_unique<PageType>()); }
    // Returns a widget by its id (do you actually need this?)
    static GUIWidget& getWidget(Identifier id);
    // Well.... It's the exact opposite of "addWidget". It accepts the ID of the Widget you get from "addWidget"
    static void removeWidget(Identifier id);

    // Function returns ImFont.
    static ImFont* getFont(std::string_view name);
    // Loads and registers a font by name. Returns true on success.
    // If rebuild is true, the font atlas is rebuilt immediately.
    static bool loadFont(std::string_view path, std::string_view name, float size = 18.0f, bool rebuild = true);
    // Sets the current default ImGui font from registered fonts.
    static bool setFont(std::string_view name);

    // Function loads an Image (PNG, JPG, EXR...) from a file. You can later get the image by calling getImage and passing your selected name to it. Please use creative names to prevent multiple GUIPages have the same Image names which would lead to mess!
    static void loadImage(std::string_view path, std::string_view name);
    // The function mentioned in the loadImage comment. Returns the imgui compatible pointer to TexturePtr! will be converted back by GUIEngine
    static ImTextureID getImage(std::string_view name);
    // Delete an image from the list. Please don't use this too often. Make GUIPages static as they won't change (like the Lobby and the Character menu) and don't load / unload all images whenever a GUIPage is launched to reduce loading times!
    static bool removeImage(std::string_view name);

    // UPDATE :)
    static void update(float delta);

    // Rebuilds the ImGui font atlas and uploads it to the renderer.
    static void rebuildFonts();

    // Loads and registers a RmlUi font face.
    static bool loadRmlFont(std::string_view path, bool fallbackFace = false);

    // RmlUi raw accessors for advanced use cases.
    static Rml::Context* getRmlContext();
    static Rml::ElementDocument* getCurrentRmlPage();
private:
    static Identifier addWidget(std::unique_ptr<GUIWidget> widget);
    static bool loadRMLGUIPage(std::unique_ptr<RMLGUIPage> page, std::string_view documentPath);
private:
    inline static int sm_imageCount;
    inline static Identifier sm_widgetCount = 0;
    inline static std::unordered_map<Identifier, std::unique_ptr<GUIWidget>> sm_widgets;
    inline static std::vector<Identifier> sm_pendingWidgetRemovals;
    inline static bool sm_isDrawingWidgets = false;
    inline static std::unique_ptr<GUIPage> sm_currentGUIPage;
    inline static std::unique_ptr<RMLGUIPage> sm_currentRMLGUIPage;
    inline static std::unordered_map<std::string, ImFont*> sm_fonts;
    inline static std::unordered_map<std::string, Texture> sm_images;
};
