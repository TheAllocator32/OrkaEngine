/*
 * File: ShaderLoader.cpp
 *
 * Loads Shader files. Adds backend extension and auto-detects split .vert/.frag files when available.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "FileLoader.hpp"
#include "../essentials/Logger.hpp"
#include "Renderer.hpp"

Shader* FileLoader::loadShader(std::string_view pathWithoutFileExtension) {
    const RenderSystemType renderSystem = Renderer::getRenderSystemType();
    if (renderSystem == RST_NONE) return nullptr;

    Shader::setActiveRenderSystem(renderSystem);
    return new Shader(Shader::fromFile(pathWithoutFileExtension, SFL_AUTO, renderSystem));
}

