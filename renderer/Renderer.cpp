/*
 * File: Renderer.hpp
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

#include "Renderer.hpp"
#include "RendererInternalTypes.hpp"
#include "Culling.hpp"
#include "../Configurator.hpp"
#include "../essentials/Threading.hpp"
#include "../essentials/Timing.hpp"
#include "../essentials/Logger.hpp"
#include "../window/Window.hpp"
#include "SMAA.hpp"
#include "SSAO.hpp"
#include "shaders/CompiledRendererShaders.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "3d/SceneManager.hpp"
#include "3d/SceneNode.hpp"
#include "3d/ModelSceneNode.hpp"
#include "3d/LightSceneNode.hpp"

#if defined(__APPLE__) && defined(__MACH__) // Metal requires single threaded renderer lol
   #define ORKA_RENDERER_SINGLE_THREADED
#endif

static RenderTarget* sg_rendererRenderTargetOverride = nullptr;
static constexpr uint16_t xg_maxBasicLights = 8;

static bool isRendererRenderTargetOverrideActive() {
    return sg_rendererRenderTargetOverride != nullptr && sg_rendererRenderTargetOverride->isValid();
}

static void clearRuntimeResources() {
    SSAO::destroyResources();
    SMAA::destroyResources();

    for (auto& kv : sg_runtimeResources.shaderPrograms) {
        if (bgfx::isValid(kv.second)) {
            bgfx::destroy(kv.second);
        }
    }
    sg_runtimeResources.shaderPrograms.clear();
    sg_runtimeResources.failedShaderLogs.clear();

    for (auto& kv : sg_runtimeResources.uniforms) {
        if (bgfx::isValid(kv.second.handle)) {
            bgfx::destroy(kv.second.handle);
        }
    }
    sg_runtimeResources.uniforms.clear();

    for (bgfx::UniformHandle sampler : sg_runtimeResources.colorSamplers) {
        if (bgfx::isValid(sampler)) {
            bgfx::destroy(sampler);
        }
    }
    sg_runtimeResources.colorSamplers.clear();

    if (bgfx::isValid(sg_runtimeResources.fallbackTexture)) {
        bgfx::destroy(sg_runtimeResources.fallbackTexture);
        sg_runtimeResources.fallbackTexture = BGFX_INVALID_HANDLE;
    }
}

static const std::vector<bgfx::UniformHandle>& ensureColorSamplers() {
    if (sg_runtimeResources.colorSamplers.empty()) {
        static constexpr const char* samplerNames[] = {
            "s_texColor",
            "s_texColor0",
            "s_texColorSampler",
            "s_texColorTexture"
        };
        for (const char* samplerName : samplerNames) {
            bgfx::UniformHandle sampler = bgfx::createUniform(samplerName, bgfx::UniformType::Sampler);
            if (bgfx::isValid(sampler)) {
                sg_runtimeResources.colorSamplers.push_back(sampler);
            }
        }
    }
    return sg_runtimeResources.colorSamplers;
}

static bgfx::TextureHandle ensureFallbackTexture() {
    if (bgfx::isValid(sg_runtimeResources.fallbackTexture)) {
        return sg_runtimeResources.fallbackTexture;
    }

    static constexpr uint32_t whitePixel = 0xffffffffu;
    const bgfx::Memory* memory = bgfx::copy(&whitePixel, sizeof(whitePixel));
    sg_runtimeResources.fallbackTexture = bgfx::createTexture2D(
        1,
        1,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE,
        memory
    );
    return sg_runtimeResources.fallbackTexture;
}

static bgfx::TextureHandle resolveMaterialTexture(Material* material) {
    if (material != nullptr) {
        for (const std::shared_ptr<Texture>& texture : material->getTextures()) {
            if (texture != nullptr && static_cast<bool>(*texture)) {
                return static_cast<bgfx::TextureHandle>(*texture);
            }
        }
    }
    return ensureFallbackTexture();
}

static bool supportsHardwareMSAA(RenderSystemType renderSystem) {
    // OpenGL2 MSAA currently resolves too bright in bgfx on our path; prefer SMAA there (Hack)
    return renderSystem != RST_OPENGL2;
}

static uint64_t resolveModelRenderState() {
    uint64_t state = xg_baseModelRenderState;
    if (!supportsHardwareMSAA(Renderer::getRequestedRenderSystemType())) {
        return state;
    }

    const std::string_view antiAliasing = OrkaConfig::ANTI_ALIASING.get();
    if (antiAliasing == "MSAA" ||
        antiAliasing == "MSAAx4" ||
        antiAliasing == "MSAAx8" ||
        antiAliasing == "MSAA+SMAA" ||
        antiAliasing == "MSAAx2+SMAA" ||
        antiAliasing == "MSAAx4+SMAA") {
        state |= BGFX_STATE_MSAA;
    }
    return state;
}

static uint32_t resolveResetFlags(RenderSystemType renderSystem) {
    uint32_t flags = (OrkaConfig::VSYNC.get() == "off") ? BGFX_RESET_NONE : BGFX_RESET_VSYNC;
    if (!supportsHardwareMSAA(renderSystem)) {
        // Keep the backbuffer non-MSAA for backends where hardware MSAA is known broken (Hack)
        return flags;
    }

    const std::string_view antiAliasing = OrkaConfig::ANTI_ALIASING.get();
    if (antiAliasing == "MSAA" ||
        antiAliasing == "MSAAx4" ||
        antiAliasing == "MSAAx4+SMAA") {
        flags |= BGFX_RESET_MSAA_X4;
    } else if (antiAliasing == "MSAAx8") {
        flags |= BGFX_RESET_MSAA_X8;
    } else if (antiAliasing == "MSAA+SMAA" ||
               antiAliasing == "MSAAx2+SMAA") {
        flags |= BGFX_RESET_MSAA_X2;
    }
    return flags;
}

static bgfx::UniformType::Enum deduceUniformType(const ShaderUniformType& value) {
    if (std::holds_alternative<matrix4x4f>(value) ||
        std::holds_alternative<matrix4x4d>(value) ||
        std::holds_alternative<matrix4x3f>(value) ||
        std::holds_alternative<matrix4x3d>(value)) {
        return bgfx::UniformType::Mat4;
    }
    return bgfx::UniformType::Vec4;
}

static bgfx::UniformHandle getOrCreateUniform(std::string_view name, bgfx::UniformType::Enum type, uint16_t num = 1) {
    if (name.empty()) {
        return BGFX_INVALID_HANDLE;
    }
    num = std::max<uint16_t>(num, 1);
    const std::string nameString(name);

    auto it = sg_runtimeResources.uniforms.find(nameString);
    if (it != sg_runtimeResources.uniforms.end()) {
        if (it->second.type != type || it->second.num != num) {
            Logger::log(LET_WARN, "Orka, Renderer", "Uniform '" + nameString + "' requested with incompatible layout. Ignoring update.");
            return BGFX_INVALID_HANDLE;
        }
        return it->second.handle;
    }

    bgfx::UniformHandle handle = bgfx::createUniform(nameString.c_str(), type, num);
    if (!bgfx::isValid(handle)) {
        return BGFX_INVALID_HANDLE;
    }

    sg_runtimeResources.uniforms[nameString] = { handle, type, num };
    return handle;
}

static float coneCos(float angleDegrees) {
    if (angleDegrees <= 0.0f) {
        return -1.0f;
    }
    return std::cos(glm::radians(angleDegrees));
}

static void applyBasicLightUniforms(const std::vector<RendererLight>& lights) {
    std::array<vector4df, xg_maxBasicLights> lightPosRange{};
    std::array<vector4df, xg_maxBasicLights> lightColorIntensity{};
    std::array<vector4df, xg_maxBasicLights> lightDirType{};
    std::array<vector4df, xg_maxBasicLights> lightSpot{};

    const uint16_t lightCount = static_cast<uint16_t>(std::min<size_t>(lights.size(), xg_maxBasicLights));
    for (uint16_t i = 0; i < lightCount; ++i) {
        const RendererLight& light = lights[i];
        const auto [r, g, b, a] = light.color.rgbaF();
        (void)a;

        lightPosRange[i] = vector4df(light.position, std::max(light.range, 0.001f));
        lightColorIntensity[i] = vector4df(r, g, b, std::max(light.intensity, 0.0f));
        lightDirType[i] = vector4df(glm::normalize(light.direction), light.type == LT_SPOT ? 1.0f : 0.0f);

        float innerCos = coneCos(light.spotInnerConeAngle);
        float outerCos = coneCos(light.spotOuterConeAngle);
        if (innerCos < outerCos) {
            std::swap(innerCos, outerCos);
        }
        lightSpot[i] = vector4df(innerCos, outerCos, 0.0f, 0.0f);
    }

    const vector4df lightParams(
        static_cast<float>(lightCount),
        lightCount > 0 ? 0.08f : 1.0f,
        0.0f,
        0.0f
    );

    const bgfx::UniformHandle posRangeUniform = getOrCreateUniform("u_lightPosRange", bgfx::UniformType::Vec4, xg_maxBasicLights);
    const bgfx::UniformHandle colorIntensityUniform = getOrCreateUniform("u_lightColorIntensity", bgfx::UniformType::Vec4, xg_maxBasicLights);
    const bgfx::UniformHandle dirTypeUniform = getOrCreateUniform("u_lightDirType", bgfx::UniformType::Vec4, xg_maxBasicLights);
    const bgfx::UniformHandle spotUniform = getOrCreateUniform("u_lightSpot", bgfx::UniformType::Vec4, xg_maxBasicLights);
    const bgfx::UniformHandle paramsUniform = getOrCreateUniform("u_lightParams", bgfx::UniformType::Vec4);

    if (bgfx::isValid(posRangeUniform)) {
        bgfx::setUniform(posRangeUniform, lightPosRange.data(), xg_maxBasicLights);
    }
    if (bgfx::isValid(colorIntensityUniform)) {
        bgfx::setUniform(colorIntensityUniform, lightColorIntensity.data(), xg_maxBasicLights);
    }
    if (bgfx::isValid(dirTypeUniform)) {
        bgfx::setUniform(dirTypeUniform, lightDirType.data(), xg_maxBasicLights);
    }
    if (bgfx::isValid(spotUniform)) {
        bgfx::setUniform(spotUniform, lightSpot.data(), xg_maxBasicLights);
    }
    if (bgfx::isValid(paramsUniform)) {
        bgfx::setUniform(paramsUniform, glm::value_ptr(lightParams));
    }
}

// Returns the nearest lights whose range intersects the model bounds.
static std::vector<RendererLight> getLightsForModel(const std::vector<RendererLight>& lights, const vector3df& center, float radius) {
    std::vector<RendererLight> result;
    std::vector<float> distances;
    result.reserve(xg_maxBasicLights);
    distances.reserve(xg_maxBasicLights);

    for (const RendererLight& light : lights) {
        const float maximumDistance = std::max(light.range, 0.0f) + radius;
        const float distanceSquared = glm::dot(light.position - center, light.position - center);
        if (distanceSquared > maximumDistance * maximumDistance) {
            continue;
        }

        if (result.size() < xg_maxBasicLights) {
            result.push_back(light);
            distances.push_back(distanceSquared);
            continue;
        }

        const auto furthest = std::max_element(distances.begin(), distances.end());
        if (distanceSquared < *furthest) {
            const size_t index = static_cast<size_t>(std::distance(distances.begin(), furthest));
            result[index] = light;
            distances[index] = distanceSquared;
        }
    }
    return result;
}

static void applyShaderUniforms(const Shader* shader) {
    if (shader == nullptr) {
        return;
    }

    for (const auto& entry : shader->getUniforms()) {
        const std::string& name = entry.first;
        const ShaderUniformType& value = entry.second;
        const bgfx::UniformType::Enum uniformType = deduceUniformType(value);
        bgfx::UniformHandle uniform = getOrCreateUniform(name, uniformType);
        if (!bgfx::isValid(uniform)) {
            continue;
        }

        if (uniformType == bgfx::UniformType::Mat4) {
            matrix4x4f matrix = matrix4x4f(1.0f);
            if (std::holds_alternative<matrix4x4f>(value)) matrix = std::get<matrix4x4f>(value);
            else if (std::holds_alternative<matrix4x4d>(value)) matrix = matrix4x4f(std::get<matrix4x4d>(value));
            else if (std::holds_alternative<matrix4x3f>(value)) matrix = matrix4x4f(std::get<matrix4x3f>(value));
            else if (std::holds_alternative<matrix4x3d>(value)) matrix = matrix4x4f(std::get<matrix4x3d>(value));
            bgfx::setUniform(uniform, glm::value_ptr(matrix));
            continue;
        }

        std::array<float, 4> values = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (std::holds_alternative<float>(value)) {
            values[0] = std::get<float>(value);
        } else if (std::holds_alternative<vector2di>(value)) {
            const vector2di vec = std::get<vector2di>(value);
            values[0] = static_cast<float>(vec.x); values[1] = static_cast<float>(vec.y);
        } else if (std::holds_alternative<vector2df>(value)) {
            const vector2df vec = std::get<vector2df>(value);
            values[0] = vec.x; values[1] = vec.y;
        } else if (std::holds_alternative<vector2dd>(value)) {
            const vector2dd vec = std::get<vector2dd>(value);
            values[0] = static_cast<float>(vec.x); values[1] = static_cast<float>(vec.y);
        } else if (std::holds_alternative<vector3di>(value)) {
            const vector3di vec = std::get<vector3di>(value);
            values[0] = static_cast<float>(vec.x); values[1] = static_cast<float>(vec.y); values[2] = static_cast<float>(vec.z);
        } else if (std::holds_alternative<vector3df>(value)) {
            const vector3df vec = std::get<vector3df>(value);
            values[0] = vec.x; values[1] = vec.y; values[2] = vec.z;
        } else if (std::holds_alternative<vector3dd>(value)) {
            const vector3dd vec = std::get<vector3dd>(value);
            values[0] = static_cast<float>(vec.x); values[1] = static_cast<float>(vec.y); values[2] = static_cast<float>(vec.z);
        } else if (std::holds_alternative<vector4di>(value)) {
            const vector4di vec = std::get<vector4di>(value);
            values[0] = static_cast<float>(vec.x); values[1] = static_cast<float>(vec.y); values[2] = static_cast<float>(vec.z); values[3] = static_cast<float>(vec.w);
        } else if (std::holds_alternative<vector4df>(value)) {
            const vector4df vec = std::get<vector4df>(value);
            values[0] = vec.x; values[1] = vec.y; values[2] = vec.z; values[3] = vec.w;
        } else if (std::holds_alternative<vector4dd>(value)) {
            const vector4dd vec = std::get<vector4dd>(value);
            values[0] = static_cast<float>(vec.x); values[1] = static_cast<float>(vec.y); values[2] = static_cast<float>(vec.z); values[3] = static_cast<float>(vec.w);
        }
        bgfx::setUniform(uniform, values.data());
    }
}

static bool readBinaryFile(std::string_view path, std::vector<uint8_t>& output) {
    output.clear();
    if (path.empty()) {
        return false;
    }
    const std::string pathString(path);

    std::ifstream input(pathString, std::ios::binary | std::ios::ate);
    if (!input) {
        return false;
    }

    std::streamsize size = input.tellg();
    if (size <= 0) {
        return false;
    }

    input.seekg(0, std::ios::beg);
    output.resize(static_cast<size_t>(size));
    if (!input.read(reinterpret_cast<char*>(output.data()), size)) {
        output.clear();
        return false;
    }
    return true;
}

static bgfx::ShaderHandle createStageShader(const Shader& shader, const ShaderStageSource& source, const char* stageName) {
    switch (source.mode) {
    case SSM_BUFFER: {
        if (source.buffer.empty()) {
            return BGFX_INVALID_HANDLE;
        }
        const bgfx::Memory* memory = bgfx::copy(source.buffer.data, source.buffer.size);
        return bgfx::createShader(memory);
    }
    case SSM_FILE: {
        std::vector<uint8_t> fileData;
        if (!readBinaryFile(source.text, fileData)) {
            Logger::log(LET_ERROR, "Orka, Renderer", std::string("Failed to read ") + stageName + " shader file: '" + source.text + "'");
            return BGFX_INVALID_HANDLE;
        }
        const bgfx::Memory* memory = bgfx::copy(fileData.data(), static_cast<uint32_t>(fileData.size()));
        return bgfx::createShader(memory);
    }
    case SSM_STRING:
        Logger::log(LET_WARN, "Orka, Renderer", std::string("Skipping ") + stageName + " shader source for '" + shader.getPath() + "'. Runtime source compilation is unsupported.");
        return BGFX_INVALID_HANDLE;
    case SSM_NONE:
    default:
        return BGFX_INVALID_HANDLE;
    }
}

static bgfx::ProgramHandle createProgramForShader(const Shader* shader) {
    if (shader == nullptr) {
        return BGFX_INVALID_HANDLE;
    }

    bgfx::ShaderHandle vertexShader = createStageShader(*shader, shader->getVertexSource(), "vertex");
    bgfx::ShaderHandle fragmentShader = createStageShader(*shader, shader->getFragmentSource(), "fragment");

    if (!bgfx::isValid(vertexShader) || !bgfx::isValid(fragmentShader)) {
        if (bgfx::isValid(vertexShader)) bgfx::destroy(vertexShader);
        if (bgfx::isValid(fragmentShader)) bgfx::destroy(fragmentShader);
        return BGFX_INVALID_HANDLE;
    }

    return bgfx::createProgram(vertexShader, fragmentShader, true);
}

static bgfx::ProgramHandle getOrCreateProgram(const Shader* shader) {
    if (shader == nullptr) {
        return BGFX_INVALID_HANDLE;
    }

    auto it = sg_runtimeResources.shaderPrograms.find(shader);
    if (it != sg_runtimeResources.shaderPrograms.end()) {
        return it->second;
    }

    bgfx::ProgramHandle program = createProgramForShader(shader);
    sg_runtimeResources.shaderPrograms[shader] = program;

    if (!bgfx::isValid(program) && !sg_runtimeResources.failedShaderLogs[shader]) {
        sg_runtimeResources.failedShaderLogs[shader] = true;
        Logger::log(LET_ERROR, "Orka, Renderer", "Failed to create shader program for '" + shader->getPath() + "'");
    }
    return program;
}

static const Shader* resolveShaderForMaterial(Material* material) {
    if (material == nullptr) {
        return &UnlitShader;
    }

    if (material->getShadingMode() == MSM_CUSTOM) {
        Shader* customShader = material->getShader();
        return customShader != nullptr ? customShader : &UnlitShader;
    }

    switch (material->getShadingMode()) {
    case MSM_BASIC:
        return &BasicLightingShader;
        break;
    case MSM_TOON:
        return &ToonShader;
        break;
    case MSM_UNLIT:
        return &UnlitShader;
        break;
    case MSM_PBR:
    case MSM_CUSTOM:
    default:
        return &UnlitShader;
        break;
    }
}

static matrix4x4f composeNodeTransform(const ModelSceneNode* node) {
    quaternion rotation = node->getRotation();
    const float rotationLength = glm::length(rotation);
    if (rotationLength <= std::numeric_limits<float>::epsilon()) {
        rotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    } else {
        rotation = glm::normalize(rotation);
    }

    const matrix4x4f translation = glm::translate(matrix4x4f(1.0f), node->getPosition());
    const matrix4x4f rotationMatrix = glm::mat4_cast(rotation);
    const matrix4x4f scale = glm::scale(matrix4x4f(1.0f), node->getScale());
    return translation * rotationMatrix * scale;
}

static void submitEntry(const SubmitEntry& entry) {
    if (entry.mesh == nullptr || entry.transforms.empty()) {
        return;
    }
    if (!bgfx::isValid(entry.mesh->getVertexBufferBGFXHandle()) || !bgfx::isValid(entry.program)) {
        return;
    }

    const auto& samplers = ensureColorSamplers();
    for (size_t i = 0; i < entry.transforms.size(); ++i) {
        const matrix4x4f& transform = entry.transforms[i];
        bgfx::setTransform(glm::value_ptr(transform));
        bgfx::setVertexBuffer(0, entry.mesh->getVertexBufferBGFXHandle());
        if (bgfx::isValid(entry.mesh->getIndexBufferBGFXHandle())) {
            bgfx::setIndexBuffer(entry.mesh->getIndexBufferBGFXHandle());
        }

        if (bgfx::isValid(entry.texture)) {
            uint8_t textureStage = 0;
            for (bgfx::UniformHandle sampler : samplers) {
                if (!bgfx::isValid(sampler)) {
                    continue;
                }
                bgfx::setTexture(textureStage, sampler, entry.texture);
                if (textureStage < UINT8_MAX) {
                    ++textureStage;
                }
            }
        }

        if (entry.shader == &BasicLightingShader || entry.shader == &ToonShader) {
            applyBasicLightUniforms(entry.lights[i]);
        }
        applyShaderUniforms(entry.shader);
        bgfx::setState(resolveModelRenderState());
        bgfx::submit(xg_rendererSceneViewID, entry.program);
    }
}

static void setupSceneView(uint32_t width, uint32_t height, const CameraSceneNode* camera, matrix4x4* outViewMatrix = nullptr, matrix4x4* outProjectionMatrix = nullptr) {
    const uint32_t targetWidth = isRendererRenderTargetOverrideActive() ? sg_rendererRenderTargetOverride->getWidth() : width;
    const uint32_t targetHeight = isRendererRenderTargetOverrideActive() ? sg_rendererRenderTargetOverride->getHeight() : height;
    const uint16_t viewWidth = static_cast<uint16_t>(std::min<uint32_t>(targetWidth, std::numeric_limits<uint16_t>::max()));
    const uint16_t viewHeight = static_cast<uint16_t>(std::min<uint32_t>(targetHeight, std::numeric_limits<uint16_t>::max()));
    const float safeAspect = (targetHeight == 0) ? 1.0f : static_cast<float>(targetWidth) / static_cast<float>(targetHeight);
    const bgfx::Caps* caps = bgfx::getCaps();
    float view[16];
    float proj[16];

    vector3df eyePosition(0.0f, 0.0f, -3.0f);
    vector3df targetPosition(0.0f, 0.0f, 0.0f);
    vector3df upDirection(0.0f, 1.0f, 0.0f);
    float fov = 60.0f;
    float nearPlane = 0.05f;
    float farPlane = 200.0f;

    if (camera != nullptr) {
        eyePosition = camera->getPosition();
        quaternion rotation = camera->getRotation();
        const float rotationLength = glm::length(rotation);
        if (rotationLength > std::numeric_limits<float>::epsilon()) {
            rotation = glm::normalize(rotation);
        } else {
            rotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
        }

        const glm::mat3 rotationBasis = glm::mat3_cast(rotation);
        const vector3df forward = rotationBasis * vector3df(0.0f, 0.0f, 1.0f);
        upDirection = rotationBasis * vector3df(0.0f, 1.0f, 0.0f);
        targetPosition = eyePosition + forward;
        fov = camera->getFoV();
        nearPlane = std::max(camera->getNearPlane(), 0.0001f);
        farPlane = std::max(camera->getFarPlane(), nearPlane + 0.0001f);
    }

    bx::mtxLookAt(
        view,
        bx::Vec3{ eyePosition.x, eyePosition.y, eyePosition.z },
        bx::Vec3{ targetPosition.x, targetPosition.y, targetPosition.z },
        bx::Vec3{ upDirection.x, upDirection.y, upDirection.z }
    );
    bx::mtxProj(proj, fov, safeAspect, nearPlane, farPlane, caps != nullptr ? caps->homogeneousDepth : true);
    if (outViewMatrix != nullptr) {
        std::memcpy(glm::value_ptr(*outViewMatrix), view, sizeof(view));
    }
    if (outProjectionMatrix != nullptr) {
        std::memcpy(glm::value_ptr(*outProjectionMatrix), proj, sizeof(proj));
    }
    bgfx::setViewMode(xg_rendererSceneViewID, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(xg_rendererSceneViewID, 0, 0, viewWidth, viewHeight);
    bgfx::setViewClear(xg_rendererSceneViewID, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x101418ff, 1.0f, 0);
    bgfx::FrameBufferHandle sceneFrameBuffer = BGFX_INVALID_HANDLE;
    if (isRendererRenderTargetOverrideActive()) {
        sceneFrameBuffer = sg_rendererRenderTargetOverride->getFrameBuffer();
    } else if (const bgfx::FrameBufferHandle SSAOSceneFrameBuffer = SSAO::getSceneFrameBuffer();
               SSAO::isEnabled() && bgfx::isValid(SSAOSceneFrameBuffer)) {
        sceneFrameBuffer = SSAOSceneFrameBuffer;
    } else if (const bgfx::FrameBufferHandle SMAASceneFrameBuffer = SMAA::getSceneFrameBuffer();
               SMAA::isEnabled() && bgfx::isValid(SMAASceneFrameBuffer)) {
        sceneFrameBuffer = SMAASceneFrameBuffer;
    }
    bgfx::setViewFrameBuffer(xg_rendererSceneViewID, sceneFrameBuffer);
    bgfx::setViewTransform(xg_rendererSceneViewID, view, proj);
    bgfx::touch(xg_rendererSceneViewID);
}

static bgfx::RendererType::Enum toBGFXRenderSystem(RenderSystemType rst) {
    // This helper converts Orka RST to BGFX RendererType
    switch (rst) {
        case RST_VULKAN:
            return bgfx::RendererType::Vulkan;
        case RST_OPENGL4:
        case RST_OPENGL2:
            return bgfx::RendererType::OpenGL;
        case RST_OPENGLES:
        case RST_WEBGL:
            return bgfx::RendererType::OpenGLES;
        case RST_DIRECTX12:
            return bgfx::RendererType::Direct3D12;
        case RST_DIRECTX11:
            return bgfx::RendererType::Direct3D11;
        case RST_METAL:
            return bgfx::RendererType::Metal;
        case RST_NONE:
            return bgfx::RendererType::Noop;
        case RST_AUTO:
        default:
            return bgfx::RendererType::Count;
    }
}


static RenderSystemType toRenderSystem(bgfx::RendererType::Enum rst) {
    switch (rst) {
        case bgfx::RendererType::Vulkan:
            return RST_VULKAN;
        case bgfx::RendererType::OpenGL:
            return RST_OPENGL4;
        case bgfx::RendererType::OpenGLES:
#if defined(__EMSCRIPTEN__)
            return RST_WEBGL;
#else
            return RST_OPENGLES;
#endif
        case bgfx::RendererType::Direct3D12:
            return RST_DIRECTX12;
        case bgfx::RendererType::Direct3D11:
            return RST_DIRECTX11;
        case bgfx::RendererType::Metal:
            return RST_METAL;
        case bgfx::RendererType::Noop:
            return RST_NONE;
        default:
            return RST_NONE;
    }
}


static RenderSystemType resolveRequestedRenderSystem(RenderSystemType rst) {
    if (rst != RST_AUTO) {
        return rst;
    }

    const std::string_view configured = OrkaConfig::RENDERSYSTEM.get();
    if (configured == "vulkan") return RST_VULKAN;
    if (configured == "opengl") return RST_OPENGL4;
    if (configured == "opengles") return RST_OPENGLES;
    if (configured == "dx11") return RST_DIRECTX11;
    if (configured == "dx12") return RST_DIRECTX12;
    if (configured == "metal") return RST_METAL;
    if (configured == "webgl") return RST_WEBGL;
    if (configured == "none") return RST_NONE;
    return RST_AUTO;
}

static bool bindSDL3WindowToBGFX(SDL_Window *window, bgfx::PlatformData &out) {
    out = {};
    if (window == nullptr) {
        return false;
    }

    const SDL_PropertiesID windowProps = SDL_GetWindowProperties(window);
    if (windowProps == 0) {
        return false;
    }

    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver == nullptr) {
        return false;
    }

    if (std::strcmp(driver, "windows") == 0) {
        out.type = bgfx::NativeWindowHandleType::Default;
        out.nwh = SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        return out.nwh != nullptr;
    }

    if (std::strcmp(driver, "wayland") == 0) {
        out.type = bgfx::NativeWindowHandleType::Wayland;
        out.ndt = SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        out.nwh = SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        return out.ndt != nullptr && out.nwh != nullptr;
    }

    if (std::strcmp(driver, "x11") == 0) {
        out.type = bgfx::NativeWindowHandleType::Default;
        out.ndt = SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        const Sint64 xid = SDL_GetNumberProperty(windowProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        out.nwh = reinterpret_cast<void *>(static_cast<uintptr_t>(xid));
        return out.ndt != nullptr && out.nwh != nullptr;
    }

    if (std::strcmp(driver, "cocoa") == 0) {
        out.type = bgfx::NativeWindowHandleType::Default;
        out.nwh = SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        if (out.nwh == nullptr) {
            Logger::log(LET_ERROR, "Orka, Renderer", "SDL cocoa window handle is null.");
            return false;
        }
        return true;
    }

    if (std::strcmp(driver, "uikit") == 0) {
        out.type = bgfx::NativeWindowHandleType::Default;
        out.nwh = SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
        return out.nwh != nullptr;
    }

    if (std::strcmp(driver, "android") == 0) {
        out.type = bgfx::NativeWindowHandleType::Default;
        out.nwh = SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
        return out.nwh != nullptr;
    }

    return false;
}

void Renderer::init(RenderSystemType rst) {
    if (sm_isInitialized) {
        deinit();
    }

    SDL_Window* window = Window::getSDLWindow();
    Logger::log(
        LET_INFO,
        "Orka, Renderer",
        std::string("SDL video driver: ") +
        (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "<null>")
    );
    if (!window) {
        sm_usedRenderSystem = RST_NONE;
        Shader::setActiveRenderSystem(sm_usedRenderSystem);
        Logger::log(LET_ERROR, "Orka, Renderer", "Renderer initialization failed! You forgot to create the Window!");
        return;
    }

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
        SDL_GetWindowSize(window, &width, &height);
    }
    if (width <= 0 || height <= 0) {
        sm_usedRenderSystem = RST_NONE;
        Shader::setActiveRenderSystem(sm_usedRenderSystem);
        Logger::log(LET_ERROR, "Orka, Renderer", "Renderer initialization failed: Invalid window size.... Hm");
        return;
    }

    bgfx::Init initInfo{};
    initInfo.debug = true;
    initInfo.platformData = {};
    const RenderSystemType resolvedRst = resolveRequestedRenderSystem(rst);
    sm_requestedRenderSystem = resolvedRst;
    initInfo.type = toBGFXRenderSystem(resolvedRst);

    initInfo.resolution.width = static_cast<uint32_t>(width);
    initInfo.resolution.height = static_cast<uint32_t>(height);
    initInfo.resolution.reset = resolveResetFlags(resolvedRst);

    if (!bindSDL3WindowToBGFX(window, initInfo.platformData)) {
        sm_usedRenderSystem = RST_NONE;
        Shader::setActiveRenderSystem(sm_usedRenderSystem);
        Logger::log(LET_ERROR, "Orka, Renderer", "Renderer initialization failed: Could not bind to SDL Window! It probably doesn't even exist...");
        return;
    }

    bgfx::setPlatformData(initInfo.platformData);

#ifdef ORKA_RENDERER_SINGLE_THREADED
    bgfx::renderFrame(); // Render single threaded!
#endif


    if (!bgfx::init(initInfo)) {
        sm_usedRenderSystem = RST_NONE;
        Shader::setActiveRenderSystem(sm_usedRenderSystem);
        Logger::log(LET_ERROR, "Orka, Renderer", std::string("BGFX init failed: ") + SDL_GetError());
        return;
    }
    
    const bgfx::RendererType::Enum actualBGFXRenderer = bgfx::getRendererType();
    sm_usedRenderSystem = toRenderSystem(actualBGFXRenderer);
    const RenderSystemType shaderRenderSystem = sm_requestedRenderSystem != RST_AUTO ? sm_requestedRenderSystem : sm_usedRenderSystem;

    sm_backbufferWidth = initInfo.resolution.width;
    sm_backbufferHeight = initInfo.resolution.height;
    sm_resetFlags = initInfo.resolution.reset;
    sm_isInitialized = true;
    Shader::setActiveRenderSystem(shaderRenderSystem);

    instantiateCompiledRendererShaders(shaderRenderSystem);

    Logger::log(
        LET_INFO,
        "Orka, Renderer",
        std::string("Renderer initialized using " + std::string(bgfx::getRendererName(actualBGFXRenderer)) + " backend")
    );
}

void Renderer::frame() {
    if (!sm_isInitialized) {
        return;
    }

    auto drawFunctionsThreadsave = sm_drawFunctions.get();
    auto& drawFunctions = *drawFunctionsThreadsave;

    float delta = Timing::getDelta("_ORENDERER");

//#ifdef ORKA_IDIOT_PROOF
    if (!Threading::amIMainThread()) {
        Logger::log(LET_ERROR, "Orka, Renderer", "You tried to call frame() outside the main thread! Let it be...");
    }
//#endif
    SDL_Window* window = Window::getSDLWindow();
    if (window != nullptr) {
        int pixelW = 0;
        int pixelH = 0;
        if (!SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH)) {
            SDL_GetWindowSize(window, &pixelW, &pixelH);
        }

        const uint32_t newWidth = static_cast<uint32_t>(std::max(pixelW, 1));
        const uint32_t newHeight = static_cast<uint32_t>(std::max(pixelH, 1));
        const uint32_t newResetFlags = resolveResetFlags(sm_requestedRenderSystem);
        if (newWidth != sm_backbufferWidth || newHeight != sm_backbufferHeight || newResetFlags != sm_resetFlags) {
            SSAO::destroyRenderTargets();
            SMAA::destroyRenderTargets();
            bgfx::reset(newWidth, newHeight, newResetFlags);
            sm_skipSMAARecreateUntilNextFrame = true;
            sm_backbufferWidth = newWidth;
            sm_backbufferHeight = newHeight;
            sm_resetFlags = newResetFlags;
            if (!SMAA::isEnabled()) {
                SMAA::destroyRenderTargets();
            }
            if (!SSAO::isEnabled()) {
                SSAO::destroyRenderTargets();
            }
            setupSceneView(sm_backbufferWidth, sm_backbufferHeight, &SceneManager::getCamera(), &sm_viewMatrix, &sm_projectionMatrix);
        }
    }

    auto invokeDrawFunctions = [&](RenderQueueOrder order) {
        auto& space = drawFunctions[order];
        std::vector<std::function<void()>> threadsaveFunctions;
        for (auto& functionAndThreadsaveStatus : space) {
            auto& function = functionAndThreadsaveStatus.fn;
            bool threadsave = functionAndThreadsaveStatus.ready;
            if (threadsave) threadsaveFunctions.emplace_back([function, delta]() { function(delta); });
            else function(delta); // Run it right away
        }
        Scheduler::parallelizeBlocking(threadsaveFunctions);
    };

    invokeDrawFunctions(RQO_FARAWAY);

    const bool ssaoActive = !isRendererRenderTargetOverrideActive() && SSAO::isEnabled() && !sm_skipSMAARecreateUntilNextFrame;
    const bool smaaActive = !isRendererRenderTargetOverrideActive() && SMAA::isEnabled() && !sm_skipSMAARecreateUntilNextFrame;

    if (ssaoActive) {
        SSAO::ensureResources(sm_backbufferWidth, sm_backbufferHeight);
    } else if (bgfx::isValid(SSAO::getSceneFrameBuffer())) {
        SSAO::destroyRenderTargets();
    }

    if (smaaActive) {
        SMAA::ensureResources(sm_backbufferWidth, sm_backbufferHeight);
        setupSceneView(sm_backbufferWidth, sm_backbufferHeight, &SceneManager::getCamera(), &sm_viewMatrix, &sm_projectionMatrix);
    } else if (bgfx::isValid(SMAA::getSceneFrameBuffer())) {
        SMAA::destroyRenderTargets();
    }

    // Render SceneNodes!
    if (SceneManager::isActive()) {
        setupSceneView(sm_backbufferWidth, sm_backbufferHeight, &SceneManager::getCamera(), &sm_viewMatrix, &sm_projectionMatrix);
        auto sceneNodes = SceneManager::sm_sceneNodes.get();
        std::vector<RendererLight> lights;
        lights.reserve(sceneNodes->size());
        for (const auto& nodeEntry : *sceneNodes) {
            auto* node = nodeEntry.second;
            if (node == nullptr || !node->isVisible() || node->getType() != SNT_LIGHT) continue;

            auto* lightNode = static_cast<LightSceneNode*>(node);
            const Light light = lightNode->getLight();
            RendererLight rendererLight;
            rendererLight.position = lightNode->getPosition();
            rendererLight.color = light.getColor();
            rendererLight.intensity = light.getIntensity();
            rendererLight.range = light.getRange();
            rendererLight.spotInnerConeAngle = light.getSpotInnerConeAngle();
            rendererLight.spotOuterConeAngle = light.getSpotOuterConeAngle();
            rendererLight.type = light.getType();

            quaternion rotation = lightNode->getRotation();
            const float rotationLength = glm::length(rotation);
            if (rotationLength > std::numeric_limits<float>::epsilon()) {
                rotation = glm::normalize(rotation);
            } else {
                rotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
            }
            rendererLight.direction = glm::mat3_cast(rotation) * vector3df(0.0f, -1.0f, 0.0f);
            lights.push_back(rendererLight);
        }

        const float aspectRatio = sm_backbufferHeight == 0 ? 1.0f : static_cast<float>(sm_backbufferWidth) / static_cast<float>(sm_backbufferHeight);
        std::unordered_map<InstanceBatchKey, SubmitEntry, InstanceBatchKeyHasher> instancedSubmissions;
        instancedSubmissions.reserve(sceneNodes->size());

        for (const auto& nodeEntry : *sceneNodes) {
            auto* node = nodeEntry.second;
            if (!node) continue;
            if (!node->isVisible()) continue;

            if (node->getType() == SNT_MODEL) { // ModelSceneNode
                auto* node2 = static_cast<ModelSceneNode*>(node);
                const Model* model = static_cast<const ModelSceneNode*>(node2)->findModel();
                if (model == nullptr) continue;
                const matrix4x4f nodeTransform = composeNodeTransform(node2);
                const BS modelBounds = model->getBS();
                if (!Culler::isSphereInCameraView(modelBounds, nodeTransform, SceneManager::getCamera(), aspectRatio)) continue;
                if (model->isDirty()) {
                    upload(model);
                }

                const vector3df modelCenter = vector3df(nodeTransform * vector4df(modelBounds.getCenter(), 1.0f));
                const float modelRadius = modelBounds.getRadius() * std::max({
                    glm::length(vector3df(nodeTransform[0])),
                    glm::length(vector3df(nodeTransform[1])),
                    glm::length(vector3df(nodeTransform[2]))
                });
                const std::vector<RendererLight> modelLights = getLightsForModel(lights, modelCenter, modelRadius);
                const auto meshes = model->getMeshes();

                auto queueSubmission = [&](Mesh* mesh, Material* material, uint32_t meshIndex, uint32_t materialIndex) {
                    if (mesh == nullptr) return;
                    if (!bgfx::isValid(mesh->getVertexBufferBGFXHandle())) return;

                    const Shader* shader = resolveShaderForMaterial(material);
                    bgfx::ProgramHandle program = getOrCreateProgram(shader);
                    if (!bgfx::isValid(program) && shader != &UnlitShader) {
                        shader = &UnlitShader;
                        program = getOrCreateProgram(shader);
                    }
                    if (!bgfx::isValid(program)) {
                        return;
                    }

                    const bgfx::TextureHandle texture = resolveMaterialTexture(material);
                    InstanceBatchKey key;
                    key.model = model;
                    key.meshIndex = meshIndex;
                    key.materialIndex = materialIndex;
                    key.shader = shader;
                    key.textureHandleIndex = texture.idx;

                    SubmitEntry& batch = instancedSubmissions[key];
                    if (batch.mesh == nullptr) {
                        batch.mesh = mesh;
                        batch.shader = shader;
                        batch.program = program;
                        batch.texture = texture;
                    }
                    batch.transforms.push_back(nodeTransform);
                    batch.lights.push_back(modelLights);
                };

                for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex) {
                    Mesh* mesh = meshes[meshIndex];
                    if (mesh == nullptr) continue;

                    auto materialsIt = model->m_materials.find(mesh);
                    if (materialsIt == model->m_materials.end() || materialsIt->second.empty()) {
                        queueSubmission(mesh, nullptr, static_cast<uint32_t>(meshIndex), std::numeric_limits<uint32_t>::max());
                        continue;
                    }

                    const auto& materials = materialsIt->second;
                    for (size_t materialIndex = 0; materialIndex < materials.size(); ++materialIndex) {
                        queueSubmission(
                            mesh,
                            materials[materialIndex],
                            static_cast<uint32_t>(meshIndex),
                            static_cast<uint32_t>(materialIndex)
                        );
                    }
                }
            }
        }

        for (const auto& batch : instancedSubmissions) {
            submitEntry(batch.second);
        }
    }

    invokeDrawFunctions(RQO_3DSPACE);

    if (ssaoActive) {
        bgfx::FrameBufferHandle compositeOutputFrameBuffer = BGFX_INVALID_HANDLE;
        if (smaaActive) {
            compositeOutputFrameBuffer = SMAA::getSceneFrameBuffer();
        }
        SSAO::runPostProcess(
            sm_backbufferWidth,
            sm_backbufferHeight,
            getOrCreateProgram(&SSAOShader),
            getOrCreateProgram(&SSAOBlurShader),
            getOrCreateProgram(&SSAOCompositeShader),
            compositeOutputFrameBuffer,
            sm_projectionMatrix
        );
    }

    if (smaaActive) {
        SMAA::runPostProcess(
            sm_backbufferWidth,
            sm_backbufferHeight,
            getOrCreateProgram(&SMAAEdgeShader),
            getOrCreateProgram(&SMAABlendWeightShader),
            getOrCreateProgram(&SMAANeighborhoodShader)
        );
    }

    invokeDrawFunctions(RQO_UISPACE);
    bgfx::frame();
    sm_skipSMAARecreateUntilNextFrame = false;
}

void Renderer::deinit() {
    if (!sm_isInitialized) {
        return;
    }

    clearRuntimeResources();
    bgfx::shutdown();
    sm_isInitialized = false;
    sm_usedRenderSystem = RST_NONE;
    sm_requestedRenderSystem = RST_AUTO;
    sm_backbufferWidth = 0;
    sm_backbufferHeight = 0;
    sm_resetFlags = BGFX_RESET_NONE;
    sm_skipSMAARecreateUntilNextFrame = false;
    sg_rendererRenderTargetOverride = nullptr;
    Shader::setActiveRenderSystem(sm_usedRenderSystem);
}

void Renderer::upload(const Model* model) {
    static const bgfx::VertexLayout layout = [] {
        bgfx::VertexLayout l;
        l.begin()
            // Position (3 floats)
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            // Normal (3 floats)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            // UV0 (2 floats)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            // Color (RGBA8)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);

#ifndef ORKA_RENDERER_DISABLE_PBR
        l
            // Tangent (3 floats)
            .add(bgfx::Attrib::Tangent, 3, bgfx::AttribType::Float)
            // UV1 (2 floats)
            .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float);
#endif

#ifndef ORKA_RENDERER_DISABLE_SKINNING
        l
            // Joints (4 uint8_t)
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8)
            // Weights (4 uint8_t normalized)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Uint8, true);
#endif

        l.end();
        return l;
    }();
    const uint16_t stride = layout.getStride();

    const auto meshes = model->getMeshes();
    for (Mesh* mesh : meshes) {
        if (mesh == nullptr) {
            continue;
        }

        auto& vertexBuffer = mesh->getVertexBuffer();
        const auto& vertices = vertexBuffer.m_vertices;
        if (!vertices.empty()) {
            if (bgfx::isValid(mesh->getVertexBufferBGFXHandle())) {
                bgfx::destroy(mesh->getVertexBufferBGFXHandle());
                mesh->setVertexBufferBGFXHandle(BGFX_INVALID_HANDLE);
            }

            std::vector<uint8_t> packed(vertices.size() * stride);
            for (size_t i = 0; i < vertices.size(); ++i) {
                const vertex& v = vertices[i];
                uint8_t* dst = packed.data() + i * stride;
                size_t offset = 0;

                std::memcpy(dst + offset, glm::value_ptr(v.position), sizeof(float) * 3);
                offset += sizeof(float) * 3;
                std::memcpy(dst + offset, glm::value_ptr(v.normal), sizeof(float) * 3);
                offset += sizeof(float) * 3;
                std::memcpy(dst + offset, glm::value_ptr(v.uv0), sizeof(float) * 2);
                offset += sizeof(float) * 2;

                const uint8_t color[4] = {
                    static_cast<uint8_t>(std::clamp(v.color.r, 0, 255)),
                    static_cast<uint8_t>(std::clamp(v.color.g, 0, 255)),
                    static_cast<uint8_t>(std::clamp(v.color.b, 0, 255)),
                    static_cast<uint8_t>(std::clamp(v.color.a, 0, 255))
                };
                std::memcpy(dst + offset, color, sizeof(color));
                offset += sizeof(color);

#ifndef ORKA_RENDERER_DISABLE_PBR
                std::memcpy(dst + offset, glm::value_ptr(v.tangent), sizeof(float) * 3);
                offset += sizeof(float) * 3;
                std::memcpy(dst + offset, glm::value_ptr(v.uv1), sizeof(float) * 2);
                offset += sizeof(float) * 2;
#endif
#ifndef ORKA_RENDERER_DISABLE_SKINNING
                uint8_t joints[4];
                for (int j = 0; j < 4; ++j) {
                    joints[j] = static_cast<uint8_t>(std::clamp(v.joints[j], 0, 255));
                }
                std::memcpy(dst + offset, joints, sizeof(joints));
                offset += sizeof(joints);

                uint8_t weights[4];
                for (int j = 0; j < 4; ++j) {
                    const int weight = std::clamp(v.weights[j], 0, 65535);
                    weights[j] = static_cast<uint8_t>((weight + 128) / 257);
                }
                std::memcpy(dst + offset, weights, sizeof(weights));
                offset += sizeof(weights);
#endif
            }

            const bgfx::Memory* mem = bgfx::copy(packed.data(), static_cast<uint32_t>(packed.size()));
            mesh->setVertexBufferBGFXHandle(bgfx::createVertexBuffer(mem, layout));
        }

        auto& indexBuffer = mesh->getIndexBuffer();
        const auto& indices = indexBuffer.m_indices;
        if (!indices.empty()) {
            if (bgfx::isValid(mesh->getIndexBufferBGFXHandle())) {
                bgfx::destroy(mesh->getIndexBufferBGFXHandle());
                mesh->setIndexBufferBGFXHandle(BGFX_INVALID_HANDLE);
            }

            const bool requires32 = std::any_of(indices.begin(), indices.end(), [](uint32_t idx) { return idx > 0xFFFFu; });
            if (requires32) {
                const bgfx::Memory* mem = bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
                mesh->setIndexBufferBGFXHandle(bgfx::createIndexBuffer(mem, BGFX_BUFFER_INDEX32));
            } else {
                std::vector<uint16_t> packedIdx(indices.size());
                for (size_t i = 0; i < indices.size(); ++i) {
                    packedIdx[i] = static_cast<uint16_t>(indices[i]);
                }
                const bgfx::Memory* mem = bgfx::copy(packedIdx.data(), static_cast<uint32_t>(packedIdx.size() * sizeof(uint16_t)));
                mesh->setIndexBufferBGFXHandle(bgfx::createIndexBuffer(mem));
            }
        }

        vertexBuffer.clearDirty();
        indexBuffer.clearDirty();
    }

    const_cast<Model*>(model)->clearDirty();
}

void Renderer::setRenderTarget(RenderTarget* renderTarget) {
    sg_rendererRenderTargetOverride = renderTarget;
}

void Renderer::resetRenderTarget() {
    sg_rendererRenderTargetOverride = nullptr;
}

Identifier Renderer::addDrawFunction(std::function<void(float)> function, RenderQueueOrder order, bool isThreadsafe) {
    auto drawFunctionsThreadsafe = sm_drawFunctions.get();
    auto nextIDThreadsafe = sm_nextDrawFunctionID.get();
    auto& drawFunctions = *drawFunctionsThreadsafe;
    Identifier& nextID = *nextIDThreadsafe;

    const Identifier id = nextID++;
    drawFunctions[order].push_back(DrawEntry{ id, std::move(function), isThreadsafe });
    return id;
}

void Renderer::removeDrawFunction(Identifier id) {
    auto drawFunctionsThreadsafe = sm_drawFunctions.get();
    auto& drawFunctions = *drawFunctionsThreadsafe;

    for (auto& orderAndFunctions : drawFunctions) {
        auto& functions = orderAndFunctions.second;
        for (auto it = functions.begin(); it != functions.end();) {
            if (it->id == id) {
                it = functions.erase(it);
            } else {
                ++it;
            }
        }
    }
}

