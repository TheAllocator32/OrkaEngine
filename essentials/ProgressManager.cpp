/*
 * File: ProgressManager.cpp
 *
 * This File contains the ProgressManager Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "ProgressManager.hpp"
#include "Logger.hpp"

void ProgressManager::init() {
    sm_IDCount=0;
    sm_elements->clear();
    auto progress = sm_progress.get();
    progress->current=0;
    progress->total=0;
    progress->elements.clear();
    progress->currentMessages.clear();
}

[[nodiscard]] Identifier ProgressManager::addElement(std::string_view name, const std::vector<std::string>& stepNames) {
    auto idCount = sm_IDCount.get();
    Identifier ID = (*idCount)++;
    ProgressElement e;
    e.ID = ID;
    e.totalLocal = stepNames.size(); // Lol
    e.messages = stepNames;
    e.currentLocal = 0;
    e.name = name;
    sm_elements->operator[](ID) = e;
    return ID;
}

void ProgressManager::step(Identifier ID, short numSteps) {
    auto elements = sm_elements.get();
    try {
        ProgressElement& e = elements->at(ID);
        auto progress = sm_progress.get();
        const short stepsToApply = std::max<short>(numSteps, 0);
        for (short i = 0; i < stepsToApply && e.currentLocal < e.totalLocal; ++i) {
            if (e.currentLocal >= 0 && e.currentLocal < static_cast<int>(e.messages.size())) {
                e.currentMessage = e.messages[e.currentLocal];
                progress->currentMessages.push_back(e.name + ": " + e.currentMessage);
                constexpr std::size_t kMaxStoredMessages = 512;
                if (progress->currentMessages.size() > kMaxStoredMessages) {
                    progress->currentMessages.erase(
                        progress->currentMessages.begin(),
                        progress->currentMessages.begin() + (progress->currentMessages.size() - kMaxStoredMessages)
                    );
                }
            }
            ++e.currentLocal;
        }
        progress->currentElement = e;
    }
    catch (std::exception& e) {
        Logger::log(LET_WARN, "Orka, Progress Manager", "Invalid progress ID '" + std::to_string(ID.getValue()) + "'!");
    }
}

void ProgressManager::removeElement(Identifier ID) {
    auto elements = sm_elements.get();
    elements->erase(ID);
}

Progress ProgressManager::getProgress() {
    auto elements = sm_elements.get();
    auto progress = sm_progress.get();
    progress->total = 0;
    progress->current = 0;
    progress->elements = *elements;
    for (const auto& [ID, element] : *elements) {
        progress->total += element.totalLocal;
        progress->current += element.currentLocal;
    }
    if (progress->total > 0) {
        progress->percent = static_cast<double>(progress->current) / progress->total * 100.0;
    } else {
        progress->percent = 0.0;
    }
    return *progress;
}

void ProgressManager::clear() {
    init(); // Yes! Do init here!
}

