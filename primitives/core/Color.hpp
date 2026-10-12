/*
 * File: Color.hpp
 *
 * Contains Color. Wow.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../../utilities/Math.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

// Since color is a primitive, we spell it snake_case. Is actually 8bit RGBA
struct Color {
    // Wow...
    int r = 0, g = 0, b = 0, a = 0;

    // Default constructor (black)
    constexpr Color() : r(0), g(0), b(0), a(255) {}

    // Constructor (rgb backward compatible)
    constexpr Color(int r, int g, int b, int a = 255) : r(r), g(g), b(b), a(a) {}

    // Constructor (float rgba in 0.0..1.0 range)
    constexpr Color(float r, float g, float b, float a = 1.0f)
        : r(Math::clamp(static_cast<int>(r * 255.0f), 0, 255)),
          g(Math::clamp(static_cast<int>(g * 255.0f), 0, 255)),
          b(Math::clamp(static_cast<int>(b * 255.0f), 0, 255)),
          a(Math::clamp(static_cast<int>(a * 255.0f), 0, 255)) {}

    // Setter (float rgba in 0.0..1.0 range)
    constexpr void set(float r, float g, float b, float a = 1.0f) {
        this->r = Math::clamp(static_cast<int>(r * 255.0f), 0, 255);
        this->g = Math::clamp(static_cast<int>(g * 255.0f), 0, 255);
        this->b = Math::clamp(static_cast<int>(b * 255.0f), 0, 255);
        this->a = Math::clamp(static_cast<int>(a * 255.0f), 0, 255);
    }

    // Setter (int rgba clamped to 8bit)
    constexpr void set(int r, int g, int b, int a = 255) {
        this->r = Math::clamp(r, 0, 255);
        this->g = Math::clamp(g, 0, 255);
        this->b = Math::clamp(b, 0, 255);
        this->a = Math::clamp(a, 0, 255);
    }

    // Setter (8bit rgba)
    constexpr void set(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = a;
    }

    // Blends a color with the current one and returns it
    constexpr Color blended(const Color& o) const {
        return {
            (r + o.r) / 2,
            (g + o.g) / 2,
            (b + o.b) / 2,
            (a + o.a) / 2
        };
    }

    // Adds 2 colors (don't expect white + white to do something...)
    constexpr Color operator+(const Color& o) const {
        return {
            Math::clampAdd(r, o.r, 255),
            Math::clampAdd(g, o.g, 255),
            Math::clampAdd(b, o.b, 255),
            Math::clampAdd(a, o.a, 255)
        };
    }

    // Subtracts 2 colors (actually pretty usefull. I guess...)
    constexpr Color operator-(const Color& o) const {
        return {
            Math::clampSub(r, o.r),
            Math::clampSub(g, o.g),
            Math::clampSub(b, o.b),
            Math::clampSub(a, o.a)
        };
    }

    // Multiplies 2 colors
    constexpr Color operator*(const Color& o) const {
        return {
            Math::modulate(r, o.r, 255),
            Math::modulate(g, o.g, 255),
            Math::modulate(b, o.b, 255),
            Math::modulate(a, o.a, 255)
        };
    }

    // DIVIDES 2 colors (WHAT THE HELL!?) 
    constexpr Color operator/(const Color& o) const {
        return {
            Math::safeDiv(r, o.r),
            Math::safeDiv(g, o.g),
            Math::safeDiv(b, o.b),
            Math::safeDiv(a, o.a)
        };
    }

    // Interpolates between 2 colors (makes nice gradients!)
    constexpr Color lerp(const Color& o, float t) const {
        return {
            Math::lerp(r, o.r, t),
            Math::lerp(g, o.g, t),
            Math::lerp(b, o.b, t),
            Math::lerp(a, o.a, t)
        };
    }

    // Returns rgba string
    std::string rgba() const;

    // Returns HEX color string. (Known as the famous #0000000 colors)
    std::string hex() const;

    // Loads the color from a uint32_t (may be HEX)
    static constexpr Color fromHex(uint32_t rgba) {
        if (rgba <= 0xFFFFFFu) {
            return { int((rgba >> 16) & 0xFF), int((rgba >> 8) & 0xFF), int(rgba & 0xFF), 255 };
        } else {
            return { int((rgba >> 24) & 0xFF), int((rgba >> 16) & 0xFF), int((rgba >> 8) & 0xFF), int(rgba & 0xFF) };
        }
    }

    // Loads the color from a HEX string
    static Color fromHex(std::string_view s);

    // Color float
    constexpr std::tuple<float, float, float, float> rgbaF() const { return { static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f }; }

    // Presets
    static constexpr Color red() { return { 255, 0, 0 }; }
    static constexpr Color green() { return { 0, 255, 0 }; }
    static constexpr Color blue() { return { 0, 0, 255 }; }
    static constexpr Color white() { return { 255, 255, 255 }; }
    static constexpr Color black() { return { 0, 0, 0 }; }
    static constexpr Color transparent() { return { 0, 0, 0, 0 }; }
};

namespace Colors {
    constexpr Color SKY() { return { 135, 206, 235 }; }
    constexpr Color GRASS() { return { 124, 252, 0 }; }
    constexpr Color SAND() { return { 194, 178, 128 }; }
    constexpr Color FIRE() { return { 255, 85, 0 }; }
    constexpr Color ICE() { return { 173, 216, 230 }; }
    constexpr Color SHADOW() { return { 50, 50, 50 }; }
    constexpr Color GOLD() { return { 255, 215, 0 }; }
    constexpr Color BLOOD() { return { 138, 3, 3 }; }
    constexpr Color LAVA() { return { 207, 16, 32 }; }
    constexpr Color NIGHT() { return { 25, 25, 112 }; }
}

enum InterpolationMode {
    IM_LINEAR,
    IM_LOGARITHMIC,
    IM_EXPONENTIAL
};

// Nice helper class for nice gradients
struct ColorGradient {
    Color from, to;
    InterpolationMode mode;

    constexpr ColorGradient(Color from, Color to, InterpolationMode mode = IM_LINEAR) : from(from), to(to), mode(mode) {}

    constexpr float apply(float t) const {
        switch (mode) {
        case IM_LINEAR:      return t;
        case IM_LOGARITHMIC: return std::log1p(t * 9.0f) / std::log1p(9.0f); // log1p(1) = ln(10)
        case IM_EXPONENTIAL: return (std::exp(t) - 1.0f) / (std::exp(1.0f) - 1.0f);
        default:             return t;
        }
    }

    constexpr Color at(float t) const {
        return from.lerp(to, apply(t));
    }
};

