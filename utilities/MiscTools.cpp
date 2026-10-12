/*
 * File: MiscTools.cpp
 *
 * Contains the definition of miscelaneous usefull functions. I'm sorry that uggly, messy code :/
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "../essentials/Logger.hpp"
#include "../essentials/HardwareJudger.hpp"
#ifndef ORKA_DISABLE_NETWORKING
#include "../network/Network.hpp"
#endif
#include "Tools.hpp"
#include <minizip/unzip.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>
#ifdef _WIN32
#include <corecrt_io.h>
#elif defined(__linux__) || defined(__APPLE__) 
#include <unistd.h>
#endif
#include "../Orka.hpp"

bool g_loggingDisabled = false;
std::streambuf* g_oldCoutBuffer;
std::streambuf* g_oldCerrBuffer;
int g_oldStdout;
int g_oldStderr;
std::ofstream g_nullStream;

using namespace std::chrono_literals;

namespace MiscTools {
    std::string formatByteSize(std::uintmax_t bytes) {
        constexpr std::uintmax_t bytesPerKiB = 1024;
        constexpr std::uintmax_t bytesPerMiB = bytesPerKiB * 1024;

        if (bytes < bytesPerKiB) {
            return std::to_string(bytes) + " B";
        }

        const bool useMiB = bytes >= bytesPerMiB;
        const double divisor = static_cast<double>(useMiB ? bytesPerMiB : bytesPerKiB);
        const double value = static_cast<double>(bytes) / divisor;

        std::ostringstream output;
        output << std::fixed << std::setprecision(value < 10.0 ? 1 : 0) << value
               << (useMiB ? " MiB" : " KiB");
        return output.str();
    }

    void shutdown(std::string_view reason, std::string_view caller, int exitCode) {
        LOG_ERROR_TYPE t;
        switch (exitCode) {
        case -1:
            t = LET_ERROR;
            break;
        case -2:
            t = LET_CRITICAL_ERROR;
            break;
        default:
            t = LET_INFO;
            break;
        }
        Logger::log(t, caller, "Shutting down... \n\t Reason: " + std::string(reason));
        OrkaEngine::shutdown(exitCode);
    }

    std::string getGameUserDataPath() {
        std::filesystem::path path;

#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        if (!appData) {
            throw std::runtime_error("Could not determine APPDATA path on Windows!");
        }
        path = std::filesystem::path(appData) / g_appName;
#elif __APPLE__
        const char* home = std::getenv("HOME");
        if (!home) {
            throw std::runtime_error("Could not determine HOME path on macOS!");
        }
        path = std::filesystem::path(home) / "Library" / "Application Support" / g_appName;
#elif __linux__
        const char* home = std::getenv("HOME");
        if (!home) {
            throw std::runtime_error("Could not determine HOME path on Linux!");
        }
        path = std::filesystem::path(home) / ".local" / "share" / g_appName;
#else
        throw std::runtime_error("Filesystem unavailable!");
#endif

        std::string str = path.string();
        if (!str.empty() && str.back() != '/' && str.back() != '\\') {
#ifdef _WIN32
            str += '\\';
#else
            str += '/';
#endif
        }

        return str;
    }


    std::string getPath(std::string_view filePath) {
        std::filesystem::path path(filePath);
        return path.parent_path().string();
    }
    std::string getFilename(std::string_view filePath) {
        std::filesystem::path path(filePath);
        return path.filename().string();
    }
    std::string getFileExtension(std::string_view filePath, bool removeDot) {
        std::filesystem::path path(filePath);
        if (removeDot && path.has_extension()) {
            return path.extension().string().substr(1);
        }
        return path.extension().string();
    }

    std::string resolveRelativePath(std::string_view rootPath, std::string_view filePath) {
        if (filePath.empty()) return {};

        const std::filesystem::path path(filePath);
        if (path.is_absolute()) {
            return path.string();
        }

        if (rootPath.empty()) {
            return path.string();
        }

        return (std::filesystem::path(rootPath) / path).string();
    }

    void disableLogging() {
        if (!g_loggingDisabled) {
            g_nullStream.open(
#ifdef _WIN32
                "NUL"
#else
                "/dev/null"
#endif
                , std::ios::out);

            g_oldCoutBuffer = std::cout.rdbuf(g_nullStream.rdbuf());
            g_oldCerrBuffer = std::cerr.rdbuf(g_nullStream.rdbuf());

            g_oldStdout = dup(fileno(stdout));
            g_oldStderr = dup(fileno(stderr));
            freopen(
#ifdef _WIN32
                "NUL"
#else
                "/dev/null"
#endif
                , "w", stdout);
            freopen(
#ifdef _WIN32
                "NUL"
#else
                "/dev/null"
#endif
                , "w", stderr);

            g_loggingDisabled = true;
        }
    }

    void enableLogging() {
        if (g_loggingDisabled) {
            std::cout.rdbuf(g_oldCoutBuffer);
            std::cerr.rdbuf(g_oldCerrBuffer);

            fflush(stdout);
            fflush(stderr);
            dup2(g_oldStdout, fileno(stdout));
            dup2(g_oldStderr, fileno(stderr));

            g_loggingDisabled = false;
        }
    }

#ifndef ORKA_DISABLE_NETWORKING
    std::string getOrDownloadInternalFolder() { // I leave it here since I maybe use it later
        if (std::filesystem::exists("internal")) {
            Logger::log(LET_INFO, "Orka", "Found internal folder. Proceeding..");
            return "internal/";
        } else if (std::filesystem::exists(getGameUserDataPath() + "internal")) {
            Logger::log(LET_INFO, "Orka", "Found internal folder. Proceeding..");
            return getGameUserDataPath() + "internal/";
        } else {
            auto configDir = getGameUserDataPath();
            Logger::log(LET_INFO, "Orka", "Downloading Runtime dependencies...");
            bool downloadRunning = true;
            Network::downloadFile(
                "https://springtechstudios.cloudns.org/SpringTechStudios/OrkaInternalFolder/releases/download/1/internal.zip",
                (configDir + "internal.zip"),
                [&downloadRunning](float p) {
                    Logger::log(LET_INFO, "Orka", "Downloading Runtime dependencies: " + std::to_string(p) + "%", false);
                    Logger::resetLine();
                    if (p >= 100.0f) {
                        downloadRunning = false;
                        Logger::log(LET_INFO, "Orka", "Downloading Runtime dependencies done ");
                    }
                });
            while (downloadRunning) {
                std::this_thread::sleep_for(100ms);
            }
            
            unzip(configDir + "internal.zip", configDir);
            std::filesystem::remove(configDir + "internal.zip");

            return configDir + "internal/";
        }
    }
#endif
    bool unzip(std::string_view zipFilePath, std::string_view targetDir) {
        const std::string zipFilePathString(zipFilePath);
        const std::string targetDirString(targetDir);
        unzFile zipfile = unzOpen(zipFilePathString.c_str());
        if (!zipfile) {
            Logger::log(LET_ERROR, "Orka", "Failed to open ZIP file!");
            return false;
        }
    
        if (unzGoToFirstFile(zipfile) != UNZ_OK) {
            unzClose(zipfile);
            return false;
        }
    
        do {
            char filename[256];
            unzGetCurrentFileInfo(zipfile, nullptr, filename, sizeof(filename), nullptr, 0, nullptr, 0);
    
            std::string fullPath = targetDirString + "/" + filename;
    
            if (filename[strlen(filename) - 1] == '/') {
                std::filesystem::create_directories(fullPath);
            } else {
                std::filesystem::create_directories(std::filesystem::path(fullPath).parent_path());
    
                if (unzOpenCurrentFile(zipfile) != UNZ_OK) continue;
    
                std::ofstream out(fullPath, std::ios::binary);
                char buffer[8192];
                int bytes;
                while ((bytes = unzReadCurrentFile(zipfile, buffer, sizeof(buffer))) > 0) {
                    out.write(buffer, bytes);
                }
    
                unzCloseCurrentFile(zipfile);
            }
        } while (unzGoToNextFile(zipfile) == UNZ_OK);
    
        unzClose(zipfile);
        return true;
    }
}

