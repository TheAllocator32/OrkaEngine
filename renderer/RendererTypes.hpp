/*
 * File: RendererTypes.hpp
 *
 * Types used by Orka Renderer and FileLoader
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once

enum RenderSystemType {
    RST_VULKAN,         // Vulkan Rendering Subsystem (standard)     --- Windows, Linux
    RST_OPENGL4,        // OpenGL3 or 4                              --- Windows, Linux, (Mac)
    RST_OPENGL2,        // OpenGL2 as fallback for toasters          --- Windows, Linux, ToasterOS     
    RST_OPENGLES,       // For android mobile devices & Toasters     --- Android, Linux, whatever
    RST_DIRECTX12,      // For our PC or XBox users                  --- Windows, XBox 
    RST_DIRECTX11,      // Fallback if DX12 not available            --- Windows, XBox
    RST_METAL,          // For our loyal Apple users ;)              --- Apple (Mac, IPhone, IPad...)
    RST_WEBGL,          // If you ever want to run Orka in Firefox   --- Browser (Firefox, Chrome)
    RST_AUTO,           // Autodetect Renders System                  --- All listed platforms
    RST_NONE            // WHAT THE HECK!?                           --- No Graphics at all!
};

enum RenderQueueOrder {
    RQO_FARAWAY,    // Drawn behind everything else (the first draw call layer)
    RQO_3DSPACE,    // Drawn within the 3d models
    RQO_UISPACE        // For UI and custom 2D stuff
};

enum TextureFiltering {
    TF_NEAREST,            // Fast but looks like pixel art
    TF_LINEAR,            // Bilinear/Trilinear -> DEFAULT
    TF_ANISOTROPIC        // Fancy stuff used in AAA. Only for real Gamers with real Gaming GPUs...
};

