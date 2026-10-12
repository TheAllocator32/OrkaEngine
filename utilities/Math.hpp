/*
 * File: Math.hpp
 *
 * Some usefull math helpers
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <span>
#include <cassert>
#include <concepts>
#include <random>
#include <glm/glm.hpp>

namespace Math {
    // Adds two values with optional clamping to max
    template<typename T>
    constexpr T clampAdd(T a, T b, T max, bool clamp = true) {
        T sum = a + b;
        return clamp ? (sum > max ? max : sum) : sum;
    }
    // Subtracts two values with optional clamping to min
    template<typename T>
    constexpr T clampSub(T a, T b, T min = static_cast<T>(0), bool clamp = true) {
        T diff = a - b;
        return clamp ? (diff < min ? min : diff) : diff;
    }
    // Multiplies two values and scales result to range
    template<typename T>
    constexpr T modulate(T a, T b, T scale, bool clamp = true) {
        T result = (a * b) / scale;
        return clamp ? (result > scale ? scale : result) : result;
    }
    // Divides two values safely, scaling result to range (default 0�255)
    template<typename T>
    constexpr T safeDiv(T a, T b, T scale = static_cast<T>(255), bool clamp = true) {
        if (b == 0) return static_cast<T>(0);
        T result = (a * scale) / b;
        return clamp ? (result > scale ? scale : result) : result;
    }
    // Linearly interpolates between two values
    template<typename T, typename F = float>
    constexpr T lerp(T a, T b, F t) {
        return static_cast<T>(a + (b - a) * t);
    }
    // Clamps a value between min and max
    template<typename T>
    constexpr T clamp(T value, T min, T max) {
        return value < min ? min : (value > max ? max : value);
    }
    // Saturates a value to 0-255 range
    template<typename T>
    constexpr T saturate(T value) {
        return clamp(value, static_cast<T>(0), static_cast<T>(255));
    }

    template<typename V>
    // std::min for vectors! Using C++23 magic
    const V& min(std::span<const V> vecs) {
        assert(!vecs.empty());

        const V* best = &vecs[0];
        auto bestLen = glm::length(*best); // Todo: Find lenght2

        for (size_t i = 1; i < vecs.size(); ++i) {
            auto len = glm::length(vecs[i]);
            if (len < bestLen) {
                best = &vecs[i];
                bestLen = len;
            }
        }
        return *best;
    }
    template<typename V>
    // std::min for vectors! Using C++23 magic
    const V& max(std::span<const V> vecs) {
        assert(!vecs.empty());

        const V* best = &vecs[0];
        auto bestLen = glm::length(*best);

        for (size_t i = 1; i < vecs.size(); ++i) {
            auto len = glm::length(vecs[i]);
            if (len > bestLen) {
                best = &vecs[i];
                bestLen = len;
            }
        }
        return *best;
    }
    template<typename V, typename... Vs>
    // Variadic vector min
    const V& min(const V& first, const Vs&... rest) {
        static_assert((std::is_same_v<V, Vs> && ...),
            "All arguments must have the same vector type");
        const V arr[] = { first, rest... };
        return min<V>(arr);
    }
    template<typename V, typename... Vs>
    // Variadic vector max
    const V& max(const V& first, const Vs&... rest) {
        static_assert((std::is_same_v<V, Vs> && ...),
            "All arguments must have the same vector type");
        const V arr[] = { first, rest... };
        return max<V>(arr);
    }

    // Generates a random number... wow. Uses system RNG (safe) or Mersenne Twister if you dare! This is also another example of ACTUALLY USEFUL C++23 concepts and template magic!
    template<typename T>
    concept primitiveNumber = std::integral<T> || std::floating_point<T>;
    template <primitiveNumber T>
    T random(T min, T max, bool safe=false) {
        if (safe) {
            std::random_device rd;
            if constexpr (std::integral<T>) {
                std::uniform_int_distribution<T> dist(min, max);
                return dist(rd);
            } else {
                std::uniform_real_distribution<T> dist(min, max);
                return dist(rd);
            }
        } else {
            static std::mt19937 gen(std::random_device{}());
            if constexpr (std::integral<T>) {
                std::uniform_int_distribution<T> dist(min, max);
                return dist(gen);
            } else {
                std::uniform_real_distribution<T> dist(min, max);
                return dist(gen);
            }
        }
    }
};

