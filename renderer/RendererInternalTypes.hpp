/*
 * File: RendererInternalTypes.hpp
 *
 * Orka's Renderer (based on bgfx) - Implementation
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
#include <bgfx/platform.h>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "../primitives/3d/Geometry.hpp"
#include "../primitives/3d/Light.hpp"
#include "../primitives/renderer/Shader.hpp"

class Model;

static constexpr uint8_t xg_rendererSceneViewID = 0;
static constexpr uint64_t xg_baseModelRenderState =
    BGFX_STATE_WRITE_RGB |
    BGFX_STATE_WRITE_A |
    BGFX_STATE_WRITE_Z |
    BGFX_STATE_DEPTH_TEST_LESS |
    BGFX_STATE_CULL_CCW;

struct CachedUniform {
    bgfx::UniformHandle handle = BGFX_INVALID_HANDLE;
    bgfx::UniformType::Enum type = bgfx::UniformType::Count;
    uint16_t num = 1;
};

struct RendererRuntimeResources {
    std::vector<bgfx::UniformHandle> colorSamplers;
    bgfx::TextureHandle fallbackTexture = BGFX_INVALID_HANDLE;
    std::unordered_map<const Shader*, bgfx::ProgramHandle> shaderPrograms;
    std::unordered_map<std::string, CachedUniform> uniforms;
    std::unordered_map<const Shader*, bool> failedShaderLogs;
};

static RendererRuntimeResources sg_runtimeResources;

struct RendererLight {
    vector3df position = vector3df(0.0f);
    vector3df direction = vector3df(0.0f, 0.0f, 1.0f);
    Color color = Color::white();
    float intensity = 1.0f;
    float range = 1.0f;
    float spotInnerConeAngle = 0.0f;
    float spotOuterConeAngle = 0.0f;
    LightType type = LT_POINT;
};

struct SubmitEntry {
    Mesh* mesh = nullptr;
    const Shader* shader = nullptr;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    std::vector<matrix4x4f> transforms;
    std::vector<std::vector<RendererLight>> lights;
};

struct InstanceBatchKey {
    const Model* model = nullptr;
    uint32_t meshIndex = 0;
    uint32_t materialIndex = 0;
    const Shader* shader = nullptr;
    uint16_t textureHandleIndex = bgfx::kInvalidHandle;

    bool operator==(const InstanceBatchKey& other) const {
        return model == other.model &&
            meshIndex == other.meshIndex &&
            materialIndex == other.materialIndex &&
            shader == other.shader &&
            textureHandleIndex == other.textureHandleIndex;
    }
};

struct InstanceBatchKeyHasher {
    inline size_t operator()(const InstanceBatchKey& key) const noexcept {
        const size_t modelPtrHash = std::hash<const void*>{}(static_cast<const void*>(key.model));
        const size_t ptrHash = std::hash<const void*>{}(static_cast<const void*>(key.shader));
        size_t seed = modelPtrHash;
        seed ^= std::hash<uint32_t>{}(key.meshIndex) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint32_t>{}(key.materialIndex) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= ptrHash + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint16_t>{}(key.textureHandleIndex) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

