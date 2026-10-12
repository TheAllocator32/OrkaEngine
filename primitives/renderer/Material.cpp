/*
 * File: Material.cpp
 *
 * Material primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Material.hpp"

Material::~Material() {
    m_textures.clear();
    if (m_shader) {
        delete m_shader;
        m_shader = nullptr;
    }
}

Material* Material::clone() const {
    Material* mat = new Material();
    mat->m_textures.reserve(m_textures.size());
    for (const std::shared_ptr<Texture>& t : m_textures) {
        if (t) mat->m_textures.push_back(std::make_shared<Texture>(*t));
    }
    mat->m_shader = m_shader ? new Shader(*m_shader) : nullptr;
    mat->m_shadingMode = m_shadingMode;
    return mat;
}

