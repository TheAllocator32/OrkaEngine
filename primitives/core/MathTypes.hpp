/*
 * File: MathTypes.hpp
 *
 * 2D/3D Primitives (vector, quaternion...)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <tuple>
#include "../../utilities/Math.hpp"

// Vector
using vector2df = glm::vec2;
using vector2di = glm::ivec2;
using vector2dd = glm::dvec2;

using vector3df = glm::vec3;
using vector3di = glm::ivec3;
using vector3dd = glm::dvec3;

using vector4df = glm::vec4;
using vector4di = glm::ivec4;
using vector4dd = glm::dvec4;

// Quaternion
using quaternionf = glm::quat;
using quaterniond = glm::dquat;
using quaternion = quaternionf;

// Matrix
using matrix4x4f = glm::mat4;
using matrix4x4d = glm::dmat4;
using matrix4x4 = matrix4x4f;

using matrix4x3f = glm::mat4x3;
using matrix4x3d = glm::dmat4x3;
using matrix4x3 = matrix4x3f;

// Degrees and radians
struct degrees;

struct radians {
    constexpr radians(float radians = 0.0f) : m_radians(radians) {}
    radians(degrees d);

    constexpr operator float() const { return m_radians; }

private:
    float m_radians = 0.0f;
};

struct degrees {
    constexpr degrees(float degrees = 0.0f) : m_degrees(degrees) {}
    degrees(radians r);

    constexpr operator float() const { return m_degrees; }

private:
    float m_degrees = 0.0f;
};

inline radians toRadians(degrees degrees) { return radians(degrees); }
inline degrees toDegrees(radians radians) { return degrees(radians); }

const float* ptr(const matrix4x4f& m);
const float* ptr(const vector3df& v);

