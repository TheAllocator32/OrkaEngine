/*
 * File: SSAO.hpp
 *
 * Minimal screen-space ambient occlusion support for Orka's bgfx renderer.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

#include "../primitives/core/MathTypes.hpp"
#include "../primitives/renderer/RenderTarget.hpp"
#include <bgfx/bgfx.h>
#include <cstdint>

class SSAO {
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
        bgfx::ProgramHandle ssaoProgram,
        bgfx::ProgramHandle blurProgram,
        bgfx::ProgramHandle compositeProgram,
        bgfx::FrameBufferHandle outputFrameBuffer,
        const matrix4x4f& projectionMatrix
    );
};

