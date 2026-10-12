/*
 * File: Tools.hpp
 *
 * Contains some usefull functions/classes that simplify things
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#ifndef ORKA_DISABLE_GUI
#include <imgui.h>
#endif
#include <fstream>

namespace MiscTools {
    // Shuts down Orka and logs the reason; exitCode -1 => error, -2 => critical.
    void shutdown(std::string_view reason, std::string_view caller, int exitCode = -1);

    // Determines the AppData/*GAMENAME* path for Windows, Linux and MacOS (something like C:/Users/Lucas/AppData/Roaming/SisijaFight)
    std::string getGameUserDataPath();

    // Gets the path of a file (e.g. data/images/image.png -> data/images)
    std::string getPath(std::string_view filePath);
    // Opposite of getPath! Gets the file name (e.g. data/images/image.png -> image.png)
    std::string getFilename(std::string_view filePath);
    // Determines the file extension (e.g. data/images/image.png -> png)
    std::string getFileExtension(std::string_view filePath, bool removeDot=true);
    // Resolves a file path relative to a root path unless the file path is already absolute.
    std::string resolveRelativePath(std::string_view rootPath, std::string_view filePath);
    // Formats a byte count using a useful unit instead of truncating small files to 0 MiB.
    std::string formatByteSize(std::uintmax_t bytes);

    // Disables logging to std::cout/cerr/printf to stop spam from other libraries
    void disableLogging();
    // Starts logging again
    void enableLogging();

    // Yup! This deflates a zip! 
    bool unzip(std::string_view zipFilePath, std::string_view targetDir);
}

#ifndef ORKA_DISABLE_GUI
namespace ImGuiTools {
    // Pass a x and y percentages and get values (based on the screen size) from it
    ImVec2 p2ToVec(float x, float y, ImVec2 customVec = ImVec2(-1, -1));
    // Pass a x percentages and get values (based on the screen size) from it
    ImVec2 pxToVec(float x);
    // Pass a y percentages and get values (based on the screen size) from it
    ImVec2 pyToVec(float y);
}
#endif

namespace PhysicsAndMathTools {
    
}

#if !defined(ORKA_DISABLE_GUI) && !defined(ORKA_DISABLE_NETWORKING)
namespace Dialogs {
    // Downloads a file and shows the File download widget
    void downloadFileWithWidget(std::string_view URL, std::string_view destination);
}
#endif

