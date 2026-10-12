/*
 * File: SMAA.hpp
 *
 * SMAA post-processing support for Orka's bgfx renderer.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

#include "../primitives/renderer/RenderTarget.hpp"
#include <bgfx/bgfx.h>
#include <cstdint>

class SMAA {
public:
    static bool isEnabled();
    static void ensureResources(uint32_t width, uint32_t height);
    static void destroyRenderTargets();
    static void destroyResources();
    static bgfx::FrameBufferHandle getSceneFrameBuffer();
    static RenderTarget* getSceneRenderTarget();
    static bool runPostProcess(
        uint32_t width,
        uint32_t height,
        bgfx::ProgramHandle edgeProgram,
        bgfx::ProgramHandle blendWeightProgram,
        bgfx::ProgramHandle neighborhoodProgram
    );
};

