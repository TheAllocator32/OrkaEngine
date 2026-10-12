/*
 * File: FileLoader.hpp
 *
 * Loads GLB/GLTF files and Images
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../primitives/3d/Geometry.hpp"
#include "../primitives/3d/Model.hpp"
#include "../primitives/renderer/Texture.hpp"
#include "ext_tiny_gltf_v3.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Static...
class FileLoader {
public:
    // Model/GLTF
    // Loads a gltf/glb file
    static Model* loadGLTFModel(std::string_view filepath, bool optimize = true, bool flipV = false, bool logSceneNodeAssets = false);
    // Loads a single mesh
    static Mesh* loadPrimitive(const tg3_model& model, const tg3_primitive& prim, bool flipV = false, bool optimize = true);

    // Image
    static Texture loadImage(std::string_view filepath, bool generateMips = true, bool premultiplyAlpha = false, bool logLargeTexture = false);

    // Shader
    // Loads a shader file and magically adds the correct extension depending on the current API. For our masterminds: instead of path/to/shader/someCrazyLightingEffect.gl.shader pass path/to/shader/someCrazyLightingEffect
    static Shader* loadShader(std::string_view pathWithoutFileExtension);

private:
    static bool buildMipChain(const uint8_t* pixels, int width, int height, bool generateMips, std::vector<uint8_t>& out);
};

