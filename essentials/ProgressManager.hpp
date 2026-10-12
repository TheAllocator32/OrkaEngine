/*
 * File: ProgressManager.hpp
 *
 * This File contains the ProgressManager - a system that controlls progresses over various subsystems!
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "Identifier.hpp"
#include "Threading.hpp"
#include <vector>
#include <string>
#include <map>

// Element
struct ProgressElement {
    Identifier ID = Identifier::INVALID;
    int currentLocal = 0;
    int totalLocal = 0;
    std::string name;
    std::vector<std::string> messages;
    std::string currentMessage;
};

// Wrapper around the progress data
struct Progress {
    int current = 0;
    int total = 0;
    float percent = 0;
    ProgressElement currentElement;
    std::map<Identifier, ProgressElement> elements;
    std::vector<std::string> currentMessages;
};

// The Progress Manager
class ProgressManager {
public:
    // Initializes the Progress Manager
    static void init();
    // Adds an element to the progress list (returns the progressID). An element may have multiple substeps that can also be named
    static Identifier addElement(std::string_view name, const std::vector<std::string>& stepNames);
    // Finish substep(s) (THREADSAFE!)
    static void step(Identifier ID, short numSteps=1);
    // Removes an element from progress tracking.
    static void removeElement(Identifier ID);
    // Returns the total Progress
    static Progress getProgress();
    // Clears all Progress Elements and their progresses
    static void clear();
private:
    inline static threadsafe<std::map<Identifier, ProgressElement>> sm_elements;
    inline static threadsafe<Progress> sm_progress;
    inline static threadsafe<Identifier> sm_IDCount;
};

