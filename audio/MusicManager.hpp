/*
 * File: MusicManager.hpp
 *
 * Parses .music.whaleml fileformat
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include "AudioEngine.hpp"

struct MusicProperties {
    std::string artist;
    std::string name;
    std::string date;
    std::string id;
    Identifier soundID;
};

// Static class that loads .music.whaleml files
class MusicManager {
public:
    // Load a .music.whaleml and stores it in the AudioEngine
    static void processFile(std::string_view path, std::string_view id);
    
    // Returns Name
    static std::string_view getName(std::string_view id);
    // Returns Artist
    static std::string_view getArtist(std::string_view id);
    // Returns the loaded Sound, or nullptr if the music ID has no active sound.
    static Sound* getSound(std::string_view id);
private:
    inline static std::unordered_map<std::string, MusicProperties> sm_music;
};

