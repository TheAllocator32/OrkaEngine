/*
 * File: PostProcessFullscreen.hpp
 *
 * Shared fullscreen post-processing helpers for Orka's bgfx renderer.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

#include <bgfx/bgfx.h>

namespace OrkaRendererInternal {
    struct FullscreenTexCoordRange {
        float minV = 0.0f;
        float maxV = 2.0f;
    };

    inline FullscreenTexCoordRange resolveFramebufferTexCoordRange() {
        const bgfx::Caps* caps = bgfx::getCaps();
        const bool originBottomLeft = caps != nullptr && caps->originBottomLeft;
        return originBottomLeft ? FullscreenTexCoordRange{ 0.0f, 2.0f } : FullscreenTexCoordRange{ 1.0f, -1.0f };
    }
}

