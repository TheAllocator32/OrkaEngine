/*
 * File: Material.hpp
 *
 * Defines the Material structure
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "Texture.hpp"
#include "Shader.hpp"

enum ModelShadingMode { // Predefined shaders!
    MSM_UNLIT,    // Solid color, always bright. Looks like MS-DOS
    MSM_BASIC,    // Very basic lighting. Looks very OpenGL2
    MSM_PBR,    // Hot stuff, looks great but requires beefy computer (default today... and Orka default unless you set ORKA_RENDERER_DISABLE_PBR)
    MSM_TOON,    // Cel shading! I personally LOVE this because I'm Nintendo Zelda BoTW/ToTK fan =)
    MSM_CUSTOM    // You have to set custom shader...
};

struct Material {
    ~Material();

    // Clones the material
    Material* clone() const;

    // Sets shader (overwrites current one but doesn't delete it)
    inline void setShader(Shader* shader) { m_shader = shader; }
    // Returns shader
    inline Shader* getShader() { return m_shader; }

    // Adds a texture
    inline void addTexture(Texture* texture) { m_textures.emplace_back(texture); }
    inline void addTexture(std::shared_ptr<Texture> texture) { if (texture) m_textures.push_back(std::move(texture)); }
    // Clears all textures
    inline void clearTextures() { m_textures.clear(); }
    // Returns all textures
    inline std::vector<std::shared_ptr<Texture>>& getTextures() { return m_textures; }
    inline const std::vector<std::shared_ptr<Texture>>& getTextures() const { return m_textures; }

    // Sets ShadingMode/Predefined shader
    inline void setShadingMode(ModelShadingMode mode) { m_shadingMode = mode; }
    // Gets ShadingMode/Predefined shader
    inline ModelShadingMode getShadingMode() const { return m_shadingMode; }

private:
    std::vector<std::shared_ptr<Texture>> m_textures;
    Shader* m_shader = nullptr;
#ifndef ORKA_RENDERER_DISABLE_PBR
    ModelShadingMode m_shadingMode = MSM_PBR;
#else
    ModelShadingMode m_shadingMode = MSM_BASIC;
#endif
};

