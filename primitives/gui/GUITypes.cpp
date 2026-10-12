/*
 * File: GUITypes.hpp
 *
 * This file implements parts of the RMLGUIPage
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "GUITypes.hpp"
#include "../../gui/GUIManager.hpp"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <string>
#include <utility>

void RMLGUIPage::ProcessEvent(Rml::Event& event) {
    Rml::Element* element = event.GetCurrentElement();
    if (element == nullptr) return;

    for (const EventCallback& registeredCallback : m_eventCallbacks) {
        if (registeredCallback.element != element || registeredCallback.eventId != event.GetId()) continue;
        registeredCallback.callback(event);
        return;
    }
}

void RMLGUIPage::onClick(std::string_view element, std::function<void()> callback) {
    if (!callback) return;

    Rml::ElementDocument* page = GUIManager::getCurrentRmlPage();
    if (page == nullptr) return;

    Rml::Element* rmlElement = page->GetElementById(std::string(element));
    if (rmlElement == nullptr) return;

    onEvent(rmlElement, Rml::EventId::Click, [callback = std::move(callback)](Rml::Event&) { callback(); });
}

void RMLGUIPage::onEvent(Rml::Element* element, Rml::EventId eventId, std::function<void(Rml::Event&)> callback) {
    if (element == nullptr || !callback) return;

    for (EventCallback& registeredCallback : m_eventCallbacks) {
        if (registeredCallback.element != element || registeredCallback.eventId != eventId) continue;
        registeredCallback.callback = std::move(callback);
        return;
    }

    m_eventCallbacks.push_back({
        .element = element,
        .eventId = eventId,
        .callback = std::move(callback)
    });
    element->AddEventListener(eventId, this);
}

void RMLGUIPage::detachEventCallbacks() {
    for (const EventCallback& registeredCallback : m_eventCallbacks) {
        if (registeredCallback.element != nullptr) registeredCallback.element->RemoveEventListener(registeredCallback.eventId, this);
    }
    m_eventCallbacks.clear();
}

