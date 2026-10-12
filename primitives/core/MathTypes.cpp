/*
 * File: MathTypes.cpp
 *
 * Math primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "MathTypes.hpp"

radians::radians(degrees d)
    : m_radians(glm::radians(static_cast<float>(d))) {}

degrees::degrees(radians r)
    : m_degrees(glm::degrees(static_cast<float>(r))) {}

const float* ptr(const matrix4x4f& m) {
    return glm::value_ptr(m);
}

const float* ptr(const vector3df& v) {
    return glm::value_ptr(v);
}

