/*
 * File: GUITypes.hpp
 *
 * This file contains the virtual classes GUIPage and GUIWidget - Contains C++23 stuff!
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../../essentials/Identifier.hpp"
#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>
#include <RmlUi/Core/EventListener.h>

// It's a Page so there can only be one GUIPage at a time
class GUIPage {
public:
    // This is called once when the GUIPage loads (by calling GUIManager::setGUIPage)! Can be used to load Images or initialize RenderTargets. Do not call manually (calling is done by GUIManager)
    virtual void load() = 0;
    // This is the actual draw method. We use ImGui so whenever you want an element to be visible call the function of the element here (NOT IN THE LOAD FUNCTION!). Also do not call manually!
    virtual void draw(float deltaTime) = 0;
    // The opposite of load. Put deinitialisation stuff there. Don't call manually (calling is done by GUIManager)
    virtual void unload() = 0;

    virtual ~GUIPage() = default;
};

// A wrapper around static GUIPages to make them feel dynamic
template<typename T>
concept StaticGUIPageConcept = requires(float dt) { T::draw(dt); };
template<StaticGUIPageConcept StaticGUIPage>
class StaticGUIPageWrapper : public GUIPage {
public:
    StaticGUIPageWrapper(bool loadOnStart = false, bool unloadAtExit = false) : m_load(loadOnStart), m_unload(unloadAtExit) {}
    void load() override { if constexpr (requires { StaticGUIPage::load(); }) if (m_load) StaticGUIPage::load(); }
    void draw(float dt) override { StaticGUIPage::draw(dt); }
    void unload() override { if constexpr (requires { StaticGUIPage::unload(); }) if (m_unload) StaticGUIPage::unload(); }
private:
    bool m_load = false, m_unload = false;
};

// Has the same subfunctions as GUIPage but GUIManager can handle multiple ones at a time. You can create popups this way
class GUIWidget {
public:
    // Sets ID (Don't call manually!)
    inline void setID(Identifier id) { m_id = id; }
    // Identical to GUIPage::load
    virtual void load()=0;
    // Identical to GUIPage::draw
    virtual void draw(float deltaTime)=0;
    // Identical to GUIPage::unload
    virtual void unload()=0;

    virtual ~GUIWidget() = default;
protected:
    Identifier m_id = Identifier::INVALID;
};

namespace Rml { class Element; class ElementDocument; class Event; };
class GUIManager;

// Usage: override load, update, unload and RML_FILE
class RMLGUIPage : public Rml::EventListener {
public:
    // Runs once at the start of the page (similar to GUIPage::load)
    virtual void load(Rml::ElementDocument* doc) = 0;
    // Runs every frame (pretty similar to GUIPage::draw)
    virtual void update(float delta, Rml::ElementDocument* doc) = 0;
    // Runs once at the destruction of the page (similar to GUIPage::unload)
    virtual void unload(Rml::ElementDocument* doc) = 0;

    // File path to the HTML page
    static constexpr std::string_view RML_FILE = ""; // Intentionally all caps; Set your filepath here (in your own pages)
protected:
    // Helper functions you may use :)
    // Registers a callback for clicks on a specified element.
    void onClick(std::string_view element, std::function<void()> callback);
    // Registers a callback for a specified RmlUi event
    void onEvent(Rml::Element* element, Rml::EventId eventId, std::function<void(Rml::Event&)> callback);
private:
    void ProcessEvent(Rml::Event& event) override;
    void detachEventCallbacks();

    struct EventCallback {
        Rml::Element* element = nullptr;
        Rml::EventId eventId = Rml::EventId::Invalid;
        std::function<void(Rml::Event&)> callback;
    };

    std::vector<EventCallback> m_eventCallbacks;

    friend class GUIManager;
};

// The same now again for static RMLUI pages
template<typename T>
concept StaticRMLGUIPageConcept = requires(float delta, Rml::ElementDocument* document) {
    { T::RML_FILE } -> std::convertible_to<std::string_view>;
    T::update(delta, document);
};
template<StaticRMLGUIPageConcept StaticRMLGUIPage>
class StaticRMLGUIPageWrapper : public RMLGUIPage {
public:
    StaticRMLGUIPageWrapper(bool loadOnStart = false, bool unloadOnExit = false) : m_load(loadOnStart), m_unload(unloadOnExit) {}
    void load(Rml::ElementDocument* document) override { if constexpr (requires { StaticRMLGUIPage::load(document); }) if (m_load) StaticRMLGUIPage::load(document); }
    void update(float delta, Rml::ElementDocument* document) override { StaticRMLGUIPage::update(delta, document); }
    void unload(Rml::ElementDocument* document) override { if constexpr (requires { StaticRMLGUIPage::unload(document); }) if (m_unload) StaticRMLGUIPage::unload(document); }
private:
    bool m_load = false, m_unload = false;
};

template <typename T>
concept GUIPageSubClass = std::derived_from<T, GUIPage>;
template <typename T>
concept GUIWidgetSubClass = std::derived_from<T, GUIWidget>;
template <typename T>
concept RMLGUIPageSubClass = std::derived_from<T, RMLGUIPage>;

