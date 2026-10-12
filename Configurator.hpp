/*
 * File: Configurator.hpp
 *
 * Stores global Orka configuration (like render system, window position, default texture sampling.
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <string_view>
#include <array>
#include <unordered_map>
#include <string>

// Used to configure global runtime stuff like the used rendering subsystem
namespace OrkaConfig {
    inline std::unordered_map<std::string_view, std::string> ng_values;
    inline constexpr std::string_view CONFIG_FILENAME = "orka.config.whaleml";

    template <std::size_t N>
    struct CVal { // ConfigVALue
        std::string_view m_key;
        std::string_view m_defaultValue;
        std::array<std::string_view, N> m_options;

        constexpr CVal(std::string_view key, std::string_view defaultValue)
            : m_key(key), m_defaultValue(defaultValue), m_options{} {}

        template <class... S>
        requires (sizeof...(S) == N)
        constexpr CVal(std::string_view key, std::string_view defaultValue, S&&... options)
            : m_key(key), m_defaultValue(defaultValue), m_options{ std::string_view{options}... } {}

        bool set(std::string_view value) const {
            if constexpr (N > 0) {
                bool isValid = false;
                for (auto option : m_options) {
                    if (option == value) {
                        isValid = true;
                        break;
                    }
                }
                if (!isValid) {
                    return false;
                }
            }
            ng_values[m_key] = std::string(value);
            return true;
        }

        std::string_view get() const {
            auto it = ng_values.find(m_key);
            if (it == ng_values.end()) {
                return m_defaultValue;
            }
            return std::string_view(it->second);
        }
    };

    template <class... S>
    CVal(std::string_view, std::string_view, S&&...) -> CVal<sizeof...(S)>;

    CVal(std::string_view, std::string_view) -> CVal<0>;

    // Rendering
    // BGFX exposes one desktop OpenGL backend; its GL version is chosen when BGFX is built.
    inline constexpr auto RENDERSYSTEM       = CVal{"rendersystem",       "auto",       "auto", "vulkan", "opengl", "opengles", "dx11", "dx12", "metal", "webgl"};
    inline constexpr auto VSYNC              = CVal{"vsync",              "magic",      "on", "off", "magic"};
    inline constexpr auto TEXTURE_FILTERING  = CVal{"texture_filtering",  "linear",     "nearest", "linear", "anisotropic"};
    inline constexpr auto AMBIENT_OCCLUSION  = CVal{"ambient_occlusion",  "SSAO",       "NONE", "SSAO", "SSAO+"};
    inline constexpr auto ANTI_ALIASING      = CVal{"anti_aliasing",      "SMAA",       "NONE", "SMAA", "MSAA", "MSAAx4", "MSAAx8", "MSAA+SMAA", "MSAAx2+SMAA", "MSAAx4+SMAA"};
    inline constexpr auto HDR_RENDERING      = CVal{"hdr_rendering",      "disable",    "enable", "disable"};
    inline constexpr auto FPS_LIMIT          = CVal{"fps_limit",          "none"};
    // Windowing
    inline constexpr auto WINDOWMODE         = CVal{"windowmode",         "windowed", "fullscreen", "windowed"};
    inline constexpr auto WINDOWSIZE         = CVal{"windowsize",         "1280×800"}; // Simple default value

    // Loads full config from file. Empty path means ./orka.config.whaleml
    void load(std::string_view path = {});
    // Writes config to file. Empty path means ./orka.config.whaleml
    void save(std::string_view path = {});
    // Restores default config
    void restoreDefault();
    // "Performance" (potato) mode. Minimizes quality, maximizes FPS, neutralizes ego.
    void tunePotato();
    // Quality/AAA mode. Maximizes quality, inverts FPS on potatoes
    void tuneDataCenter();
}

