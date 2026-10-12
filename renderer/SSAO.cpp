/*
 * File: SSAO.cpp
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

#include "SSAO.hpp"

#include "../Configurator.hpp"
#include "../essentials/Logger.hpp"
#include "PostProcessFullscreen.hpp"

#include <algorithm>
#include <limits>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace OrkaSSAOInternal {
    constexpr uint8_t g_rendererSSAOViewID = 1;
    constexpr uint8_t g_rendererSSAOBlurViewID = 2;
    constexpr uint8_t g_rendererSSAOSecondBlurViewID = 3;
    constexpr uint8_t g_rendererSSAOCompositeViewID = 4;
    
    constexpr uint64_t g_SSAOSamplerFlags =
        BGFX_SAMPLER_U_CLAMP |
        BGFX_SAMPLER_V_CLAMP;
    
    constexpr uint64_t g_SSAOPointSamplerFlags =
        g_SSAOSamplerFlags |
        BGFX_SAMPLER_MIN_POINT |
        BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_MIP_POINT;
    
    constexpr uint64_t g_SSAORenderTargetFlags =
        BGFX_TEXTURE_RT |
        g_SSAOSamplerFlags;
    
    constexpr uint64_t g_SSAODepthTargetFlags =
        BGFX_TEXTURE_RT |
        g_SSAOPointSamplerFlags;
    
    struct SSAOFullscreenVertex {
        float x;
        float y;
        float z;
        float u;
        float v;
    };
    
    struct SSAOState {
        RenderTarget sceneTarget;
        RenderTarget occlusionTarget;
        RenderTarget blurTarget;
        bgfx::UniformHandle paramsUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle qualityUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle optionsUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle invProjectionUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle depthSampler = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle sceneSampler = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle aoSampler = BGFX_INVALID_HANDLE;
        bgfx::VertexLayout fullscreenLayout;
        bool fullscreenLayoutReady = false;
    };
    
    SSAOState g_SSAOState;
    
    void ensureSSAOFullscreenLayout() {
        if (g_SSAOState.fullscreenLayoutReady) {
            return;
        }
    
        g_SSAOState.fullscreenLayout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        g_SSAOState.fullscreenLayoutReady = true;
    }
    
    void ensureSSAOUniforms() {
        if (!bgfx::isValid(g_SSAOState.paramsUniform)) {
            g_SSAOState.paramsUniform = bgfx::createUniform("u_ssaoParams", bgfx::UniformType::Vec4);
        }
        if (!bgfx::isValid(g_SSAOState.qualityUniform)) {
            g_SSAOState.qualityUniform = bgfx::createUniform("u_ssaoQuality", bgfx::UniformType::Vec4);
        }
        if (!bgfx::isValid(g_SSAOState.optionsUniform)) {
            g_SSAOState.optionsUniform = bgfx::createUniform("u_ssaoOptions", bgfx::UniformType::Vec4);
        }
        if (!bgfx::isValid(g_SSAOState.invProjectionUniform)) {
            g_SSAOState.invProjectionUniform = bgfx::createUniform("u_ssaoInvProjection", bgfx::UniformType::Mat4);
        }
        if (!bgfx::isValid(g_SSAOState.depthSampler)) {
            g_SSAOState.depthSampler = bgfx::createUniform("s_depthTex", bgfx::UniformType::Sampler);
        }
        if (!bgfx::isValid(g_SSAOState.sceneSampler)) {
            g_SSAOState.sceneSampler = bgfx::createUniform("s_sceneTex", bgfx::UniformType::Sampler);
        }
        if (!bgfx::isValid(g_SSAOState.aoSampler)) {
            g_SSAOState.aoSampler = bgfx::createUniform("s_aoTex", bgfx::UniformType::Sampler);
        }
    }
    
    void ensureSSAORenderTargets(uint32_t width, uint32_t height) {
        width = std::max<uint32_t>(width, 1);
        height = std::max<uint32_t>(height, 1);
    
        const bool sceneReady = g_SSAOState.sceneTarget.ensure(width, height, {
            RenderTargetAttachment{ bgfx::TextureFormat::RGBA8, g_SSAORenderTargetFlags },
            RenderTargetAttachment{ bgfx::TextureFormat::D24S8, g_SSAODepthTargetFlags }
        });
        const bool occlusionReady = g_SSAOState.occlusionTarget.ensure(width, height, bgfx::TextureFormat::RGBA8, g_SSAORenderTargetFlags);
        const bool blurReady = g_SSAOState.blurTarget.ensure(width, height, bgfx::TextureFormat::RGBA8, g_SSAORenderTargetFlags);
    
        if (!sceneReady || !occlusionReady || !blurReady) {
            SSAO::destroyRenderTargets();
            Logger::log(LET_ERROR, "Orka, Renderer", "Failed to create SSAO render targets.");
        }
    }
    
    bool submitSSAOFullscreenTriangle(uint8_t viewID, bgfx::ProgramHandle program) {
        if (!bgfx::isValid(program)) {
            return false;
        }
    
        ensureSSAOFullscreenLayout();
        if (bgfx::getAvailTransientVertexBuffer(3, g_SSAOState.fullscreenLayout) < 3) {
            return false;
        }
    
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, 3, g_SSAOState.fullscreenLayout);
        auto* vertices = reinterpret_cast<SSAOFullscreenVertex*>(tvb.data);
        const OrkaRendererInternal::FullscreenTexCoordRange texCoordRange = OrkaRendererInternal::resolveFramebufferTexCoordRange();
        vertices[0] = { -1.0f, -1.0f, 0.0f, 0.0f, texCoordRange.minV };
        vertices[1] = {  3.0f, -1.0f, 0.0f, 2.0f, texCoordRange.minV };
        vertices[2] = { -1.0f,  3.0f, 0.0f, 0.0f, texCoordRange.maxV };
    
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(viewID, program);
        return true;
    }
    
    void setupSSAOPostProcessViews(uint32_t width, uint32_t height, bgfx::FrameBufferHandle outputFrameBuffer, bool extraQuality) {
        const uint16_t viewWidth = static_cast<uint16_t>(std::min<uint32_t>(width, std::numeric_limits<uint16_t>::max()));
        const uint16_t viewHeight = static_cast<uint16_t>(std::min<uint32_t>(height, std::numeric_limits<uint16_t>::max()));
    
        bgfx::setViewMode(g_rendererSSAOViewID, bgfx::ViewMode::Sequential);
        bgfx::setViewRect(g_rendererSSAOViewID, 0, 0, viewWidth, viewHeight);
        bgfx::setViewClear(g_rendererSSAOViewID, BGFX_CLEAR_COLOR, 0xffffffff, 1.0f, 0);
        bgfx::setViewFrameBuffer(g_rendererSSAOViewID, g_SSAOState.occlusionTarget.getFrameBuffer());
        bgfx::touch(g_rendererSSAOViewID);
    
        bgfx::setViewMode(g_rendererSSAOBlurViewID, bgfx::ViewMode::Sequential);
        bgfx::setViewRect(g_rendererSSAOBlurViewID, 0, 0, viewWidth, viewHeight);
        bgfx::setViewClear(g_rendererSSAOBlurViewID, BGFX_CLEAR_COLOR, 0xffffffff, 1.0f, 0);
        bgfx::setViewFrameBuffer(g_rendererSSAOBlurViewID, g_SSAOState.blurTarget.getFrameBuffer());
        bgfx::touch(g_rendererSSAOBlurViewID);
    
        if (extraQuality) {
            bgfx::setViewMode(g_rendererSSAOSecondBlurViewID, bgfx::ViewMode::Sequential);
            bgfx::setViewRect(g_rendererSSAOSecondBlurViewID, 0, 0, viewWidth, viewHeight);
            bgfx::setViewClear(g_rendererSSAOSecondBlurViewID, BGFX_CLEAR_COLOR, 0xffffffff, 1.0f, 0);
            bgfx::setViewFrameBuffer(g_rendererSSAOSecondBlurViewID, g_SSAOState.occlusionTarget.getFrameBuffer());
            bgfx::touch(g_rendererSSAOSecondBlurViewID);
        }
    
        bgfx::setViewMode(g_rendererSSAOCompositeViewID, bgfx::ViewMode::Sequential);
        bgfx::setViewRect(g_rendererSSAOCompositeViewID, 0, 0, viewWidth, viewHeight);
        bgfx::setViewClear(g_rendererSSAOCompositeViewID, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
        bgfx::setViewFrameBuffer(g_rendererSSAOCompositeViewID, outputFrameBuffer);
        bgfx::touch(g_rendererSSAOCompositeViewID);
    }
}

bool SSAO::isEnabled() {
    const std::string_view ambientOcclusion = OrkaConfig::AMBIENT_OCCLUSION.get();
    return ambientOcclusion == "SSAO" || ambientOcclusion == "SSAO+";
}

void SSAO::destroyRenderTargets() {
    OrkaSSAOInternal::g_SSAOState.sceneTarget.destroy();
    OrkaSSAOInternal::g_SSAOState.occlusionTarget.destroy();
    OrkaSSAOInternal::g_SSAOState.blurTarget.destroy();
}

void SSAO::destroyResources() {
    destroyRenderTargets();

    bgfx::UniformHandle* handles[] = {
        &OrkaSSAOInternal::g_SSAOState.paramsUniform,
        &OrkaSSAOInternal::g_SSAOState.qualityUniform,
        &OrkaSSAOInternal::g_SSAOState.optionsUniform,
        &OrkaSSAOInternal::g_SSAOState.invProjectionUniform,
        &OrkaSSAOInternal::g_SSAOState.depthSampler,
        &OrkaSSAOInternal::g_SSAOState.sceneSampler,
        &OrkaSSAOInternal::g_SSAOState.aoSampler
    };
    for (bgfx::UniformHandle* handle : handles) {
        if (bgfx::isValid(*handle)) {
            bgfx::destroy(*handle);
            *handle = BGFX_INVALID_HANDLE;
        }
    }
}

void SSAO::ensureResources(uint32_t width, uint32_t height) {
    OrkaSSAOInternal::ensureSSAOFullscreenLayout();
    OrkaSSAOInternal::ensureSSAOUniforms();
    OrkaSSAOInternal::ensureSSAORenderTargets(width, height);
}

bgfx::FrameBufferHandle SSAO::getSceneFrameBuffer() {
    return OrkaSSAOInternal::g_SSAOState.sceneTarget.getFrameBuffer();
}

RenderTarget* SSAO::getSceneRenderTarget() {
    return &OrkaSSAOInternal::g_SSAOState.sceneTarget;
}

bool SSAO::runPostProcess(uint32_t width, uint32_t height, bgfx::ProgramHandle ssaoProgram, bgfx::ProgramHandle blurProgram, bgfx::ProgramHandle compositeProgram, bgfx::FrameBufferHandle outputFrameBuffer, const matrix4x4f& projectionMatrix) {
    ensureResources(width, height);
    if (!OrkaSSAOInternal::g_SSAOState.sceneTarget.isValid() ||
        !OrkaSSAOInternal::g_SSAOState.occlusionTarget.isValid() ||
        !OrkaSSAOInternal::g_SSAOState.blurTarget.isValid()) {
        return false;
    }

    const bool extraQuality = OrkaConfig::AMBIENT_OCCLUSION.get() == "SSAO+";
    OrkaSSAOInternal::setupSSAOPostProcessViews(width, height, outputFrameBuffer, extraQuality);
    const float params[] = {
        1.0f / static_cast<float>(std::max<uint32_t>(width, 1)),
        1.0f / static_cast<float>(std::max<uint32_t>(height, 1)),
        1.65f,
        2.25f
    };
    const bgfx::Caps* caps = bgfx::getCaps();
    const bool homogeneousDepth = caps != nullptr ? caps->homogeneousDepth : true;
    const float quality[] = {
        0.045f,
        extraQuality ? 58.0f : 46.0f,
        homogeneousDepth ? 2.0f : 1.0f,
        homogeneousDepth ? -1.0f : 0.0f
    };
    const float options[] = {
        extraQuality ? 32.0f : 16.0f,
        extraQuality ? 2.0f : 1.0f,
        0.0f,
        0.0f
    };
    const matrix4x4f invProjectionMatrix = glm::inverse(projectionMatrix);

    const bgfx::TextureHandle sceneTexture = OrkaSSAOInternal::g_SSAOState.sceneTarget.getTexture(0);
    const bgfx::TextureHandle depthTexture = OrkaSSAOInternal::g_SSAOState.sceneTarget.getTexture(1);
    const bgfx::TextureHandle occlusionTexture = OrkaSSAOInternal::g_SSAOState.occlusionTarget.getTexture(0);
    const bgfx::TextureHandle blurTexture = OrkaSSAOInternal::g_SSAOState.blurTarget.getTexture(0);

    bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.paramsUniform, params);
    bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.qualityUniform, quality);
    bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.optionsUniform, options);
    bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.invProjectionUniform, glm::value_ptr(invProjectionMatrix));
    bgfx::setTexture(0, OrkaSSAOInternal::g_SSAOState.depthSampler, depthTexture, OrkaSSAOInternal::g_SSAOPointSamplerFlags);
    OrkaSSAOInternal::submitSSAOFullscreenTriangle(OrkaSSAOInternal::g_rendererSSAOViewID, ssaoProgram);

    bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.paramsUniform, params);
    bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.qualityUniform, quality);
    bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.optionsUniform, options);
    bgfx::setTexture(0, OrkaSSAOInternal::g_SSAOState.aoSampler, occlusionTexture, OrkaSSAOInternal::g_SSAOSamplerFlags);
    bgfx::setTexture(1, OrkaSSAOInternal::g_SSAOState.depthSampler, depthTexture, OrkaSSAOInternal::g_SSAOPointSamplerFlags);
    OrkaSSAOInternal::submitSSAOFullscreenTriangle(OrkaSSAOInternal::g_rendererSSAOBlurViewID, blurProgram);

    bgfx::TextureHandle finalAOTexture = blurTexture;
    if (extraQuality) {
        bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.paramsUniform, params);
        bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.qualityUniform, quality);
        bgfx::setUniform(OrkaSSAOInternal::g_SSAOState.optionsUniform, options);
        bgfx::setTexture(0, OrkaSSAOInternal::g_SSAOState.aoSampler, blurTexture, OrkaSSAOInternal::g_SSAOSamplerFlags);
        bgfx::setTexture(1, OrkaSSAOInternal::g_SSAOState.depthSampler, depthTexture, OrkaSSAOInternal::g_SSAOPointSamplerFlags);
        OrkaSSAOInternal::submitSSAOFullscreenTriangle(OrkaSSAOInternal::g_rendererSSAOSecondBlurViewID, blurProgram);
        finalAOTexture = occlusionTexture;
    }

    bgfx::setTexture(0, OrkaSSAOInternal::g_SSAOState.sceneSampler, sceneTexture, OrkaSSAOInternal::g_SSAOSamplerFlags);
    bgfx::setTexture(1, OrkaSSAOInternal::g_SSAOState.aoSampler, finalAOTexture, OrkaSSAOInternal::g_SSAOSamplerFlags);
    OrkaSSAOInternal::submitSSAOFullscreenTriangle(OrkaSSAOInternal::g_rendererSSAOCompositeViewID, compositeProgram);
    return true;
}

