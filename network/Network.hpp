/*
 * File: Network.hpp
 *
 * Async HTTPS and legacy HTTP client
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
#include <functional>

class Network {
public:
    // Async GET (use https://domain.ending/path URL just as in the browser)
    static void GET(std::string_view url, std::function<void(std::string, int)> finish);
    // Async POST (use https://domain.ending/path URL just as in the browser)
    static void POST(std::string_view url, std::string_view body, std::string_view contentType = "application/x-www-form-urlencoded", std::function<void(std::string, int)> finish=0);
    // Downloads a file (the progress is in %) (use https://domain.ending/path URL just as in the browser)
    static void downloadFile(std::string_view url, std::string_view file_path, std::function<void(float)> progress=0, std::function<void(bool)> finish=0);

    // legacy GET (use https://domain.ending/path URL just as in the browser)
    static std::string GETSync(std::string_view url);
    // legacy POST (use https://domain.ending/path URL just as in the browser)
    static std::string POSTSync(std::string_view url, std::string_view body, std::string_view contentType = "application/x-www-form-urlencoded");
    // pretty useless... (use https://domain.ending/path URL just as in the browser)
    static void downloadFileSync(std::string_view url, std::string_view file_path);
};

