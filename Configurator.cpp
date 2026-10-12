/*
 * File: Configurator.cpp
 *
 * Stores global Orka configuration (like render system, window position, default texture sampling. - Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Configurator.hpp"
#include "whaleml/WhaleMLFile.hpp"
#include "essentials/Logger.hpp"
#include <filesystem>

namespace OrkaConfig {
    void load(std::string_view path) {
        std::filesystem::path configPath;
        if (!path.empty()) {
            configPath = std::filesystem::path(path);
        } else {
            configPath = std::filesystem::current_path() / std::string(CONFIG_FILENAME);
        }

        if (!std::filesystem::exists(configPath)) {
            Logger::log(LET_WARN, "Orka, Configurator", "Config file '" + configPath.string() + "' does not exist. Keeping current settings.");
            return;
        }

        WhaleMLFile db;
        db.load(configPath.string());

        RENDERSYSTEM.set(db.string("rendering.rendersystem", RENDERSYSTEM.get()));
        VSYNC.set(db.string("rendering.vsync", VSYNC.get()));
        TEXTURE_FILTERING.set(db.string("rendering.texture_filtering", TEXTURE_FILTERING.get()));
        AMBIENT_OCCLUSION.set(db.string("rendering.ambient_occlusion", AMBIENT_OCCLUSION.get()));
        ANTI_ALIASING.set(db.string("rendering.anti_aliasing", ANTI_ALIASING.get()));
        HDR_RENDERING.set(db.string("rendering.hdr_rendering", HDR_RENDERING.get()));
        FPS_LIMIT.set(db.string("rendering.fps_limit", FPS_LIMIT.get()));
        WINDOWMODE.set(db.string("window.mode", WINDOWMODE.get()));
        WINDOWSIZE.set(db.string("window.size", WINDOWSIZE.get()));
    }

    void save(std::string_view path) {
        std::filesystem::path configPath;
        if (!path.empty()) {
            configPath = std::filesystem::path(path);
        } else {
            configPath = std::filesystem::current_path() / std::string(CONFIG_FILENAME);
        }

        WhaleMLFile db;

        db.setValue("rendering.rendersystem", RENDERSYSTEM.get());
        db.setValue("rendering.vsync", VSYNC.get());
        db.setValue("rendering.texture_filtering", TEXTURE_FILTERING.get());
        db.setValue("rendering.ambient_occlusion", AMBIENT_OCCLUSION.get());
        db.setValue("rendering.anti_aliasing", ANTI_ALIASING.get());
        db.setValue("rendering.hdr_rendering", HDR_RENDERING.get());
        db.setValue("rendering.fps_limit", FPS_LIMIT.get());
        db.setValue("window.mode", WINDOWMODE.get());
        db.setValue("window.size", WINDOWSIZE.get());

        db.dumpFile(configPath.string(), WPO_TIDY, WPO_TRIM_QUOTES);
    }

    void restoreDefault() {
        ng_values.clear();
    }

    void tunePotato() {
        TEXTURE_FILTERING.set("nearest");
        HDR_RENDERING.set("disable");
        AMBIENT_OCCLUSION.set("NONE");
        ANTI_ALIASING.set("NONE");
        VSYNC.set("off");
    }

    void tuneDataCenter() {
        TEXTURE_FILTERING.set("anisotropic");
        HDR_RENDERING.set("enable");
        AMBIENT_OCCLUSION.set("SSAO+");
        ANTI_ALIASING.set("MSAAx4+SMAA");
        VSYNC.set("on");
    }
}

