/*
 * File: SMAA.cpp
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

#include "SMAA.hpp"

#include "../Configurator.hpp"
#include "../essentials/Logger.hpp"
#include "PostProcessFullscreen.hpp"
#include "shaders/smaa/AreaTex.h"
#include "shaders/smaa/SearchTex.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace OrkaSMAAInternal {
    constexpr uint8_t g_rendererSMAAEdgeViewID = 5;
    constexpr uint8_t g_rendererSMAABlendWeightViewID = 6;
    constexpr uint8_t g_rendererSMAANeighborhoodViewID = 7;
    
    constexpr uint64_t g_SMAASamplerFlags =
        BGFX_SAMPLER_U_CLAMP |
        BGFX_SAMPLER_V_CLAMP;
    
    constexpr uint64_t g_SMAARenderTargetFlags =
        BGFX_TEXTURE_RT |
        g_SMAASamplerFlags;
    
    struct SMAAFullscreenVertex {
        float x;
        float y;
        float z;
        float u;
        float v;
    };
    
    struct SMAAState {
        RenderTarget sceneTarget;
        RenderTarget edgesTarget;
        RenderTarget blendTarget;
        bgfx::TextureHandle areaTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle searchTexture = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle rtMetricsUniform = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle colorSampler = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle edgesSampler = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle areaSampler = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle searchSampler = BGFX_INVALID_HANDLE;
        bgfx::UniformHandle blendSampler = BGFX_INVALID_HANDLE;
        bgfx::VertexLayout fullscreenLayout;
        bool fullscreenLayoutReady = false;
    };
    
    SMAAState g_SMAAState;
    
    void ensureSMAAFullscreenLayout() {
        if (g_SMAAState.fullscreenLayoutReady) {
            return;
        }
    
        g_SMAAState.fullscreenLayout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        g_SMAAState.fullscreenLayoutReady = true;
    }
    
    void ensureSMAAUniforms() {
        if (!bgfx::isValid(g_SMAAState.rtMetricsUniform)) {
            g_SMAAState.rtMetricsUniform = bgfx::createUniform("u_smaaRtMetrics", bgfx::UniformType::Vec4);
        }
        if (!bgfx::isValid(g_SMAAState.colorSampler)) {
            g_SMAAState.colorSampler = bgfx::createUniform("s_colorTex", bgfx::UniformType::Sampler);
        }
        if (!bgfx::isValid(g_SMAAState.edgesSampler)) {
            g_SMAAState.edgesSampler = bgfx::createUniform("s_edgesTex", bgfx::UniformType::Sampler);
        }
        if (!bgfx::isValid(g_SMAAState.areaSampler)) {
            g_SMAAState.areaSampler = bgfx::createUniform("s_areaTex", bgfx::UniformType::Sampler);
        }
        if (!bgfx::isValid(g_SMAAState.searchSampler)) {
            g_SMAAState.searchSampler = bgfx::createUniform("s_searchTex", bgfx::UniformType::Sampler);
        }
        if (!bgfx::isValid(g_SMAAState.blendSampler)) {
            g_SMAAState.blendSampler = bgfx::createUniform("s_blendTex", bgfx::UniformType::Sampler);
        }
    }
    
    bgfx::TextureHandle createSMAAAreaTexture() {
        std::vector<uint8_t> rgba(static_cast<size_t>(AREATEX_WIDTH) * static_cast<size_t>(AREATEX_HEIGHT) * 4u);
        for (size_t src = 0, dst = 0; src + 1 < static_cast<size_t>(AREATEX_SIZE); src += 2, dst += 4) {
            rgba[dst + 0] = areaTexBytes[src + 0];
            rgba[dst + 1] = areaTexBytes[src + 1];
            rgba[dst + 2] = 0;
            rgba[dst + 3] = 255;
        }
    
        const bgfx::Memory* memory = bgfx::copy(rgba.data(), static_cast<uint32_t>(rgba.size()));
        return bgfx::createTexture2D(
            AREATEX_WIDTH,
            AREATEX_HEIGHT,
            false,
            1,
            bgfx::TextureFormat::RGBA8,
            g_SMAASamplerFlags,
            memory
        );
    }
    
    bgfx::TextureHandle createSMAASearchTexture() {
        std::vector<uint8_t> rgba(static_cast<size_t>(SEARCHTEX_WIDTH) * static_cast<size_t>(SEARCHTEX_HEIGHT) * 4u);
        for (size_t src = 0, dst = 0; src < static_cast<size_t>(SEARCHTEX_SIZE); ++src, dst += 4) {
            rgba[dst + 0] = searchTexBytes[src];
            rgba[dst + 1] = 0;
            rgba[dst + 2] = 0;
            rgba[dst + 3] = 255;
        }
    
        const bgfx::Memory* memory = bgfx::copy(rgba.data(), static_cast<uint32_t>(rgba.size()));
        return bgfx::createTexture2D(
            SEARCHTEX_WIDTH,
            SEARCHTEX_HEIGHT,
            false,
            1,
            bgfx::TextureFormat::RGBA8,
            g_SMAASamplerFlags,
            memory
        );
    }
    
    void ensureSMAALookupTextures() {
        if (!bgfx::isValid(g_SMAAState.areaTexture)) {
            g_SMAAState.areaTexture = createSMAAAreaTexture();
        }
        if (!bgfx::isValid(g_SMAAState.searchTexture)) {
            g_SMAAState.searchTexture = createSMAASearchTexture();
        }
    }
    
    uint64_t resolveSMAASceneRenderTargetFlags() {
        // SMAA samples this texture in the edge pass, so it must stay sampleable.
        return g_SMAARenderTargetFlags;
    }
    
    uint64_t resolveSMAASceneDepthTargetFlags() {
        return BGFX_TEXTURE_RT;
    }
    
    void ensureSMAARenderTargets(uint32_t width, uint32_t height) {
        width = std::max<uint32_t>(width, 1);
        height = std::max<uint32_t>(height, 1);
    
        const bool sceneReady = g_SMAAState.sceneTarget.ensure(width, height, {
            RenderTargetAttachment{ bgfx::TextureFormat::RGBA8, resolveSMAASceneRenderTargetFlags() },
            RenderTargetAttachment{ bgfx::TextureFormat::D24S8, resolveSMAASceneDepthTargetFlags() }
        });
        const bool edgesReady = g_SMAAState.edgesTarget.ensure(width, height, bgfx::TextureFormat::RGBA8, g_SMAARenderTargetFlags);
        const bool blendReady = g_SMAAState.blendTarget.ensure(width, height, bgfx::TextureFormat::RGBA8, g_SMAARenderTargetFlags);
    
        if (!sceneReady || !edgesReady || !blendReady) {
            SMAA::destroyRenderTargets();
            Logger::log(LET_ERROR, "Orka, Renderer", "Failed to create SMAA render targets.");
        }
    }
    
    bool submitSMAAFullscreenTriangle(uint8_t viewID, bgfx::ProgramHandle program) {
        if (!bgfx::isValid(program)) {
            return false;
        }
    
        ensureSMAAFullscreenLayout();
        if (bgfx::getAvailTransientVertexBuffer(3, g_SMAAState.fullscreenLayout) < 3) {
            return false;
        }
    
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, 3, g_SMAAState.fullscreenLayout);
        auto* vertices = reinterpret_cast<SMAAFullscreenVertex*>(tvb.data);
        const OrkaRendererInternal::FullscreenTexCoordRange texCoordRange = OrkaRendererInternal::resolveFramebufferTexCoordRange();
        vertices[0] = { -1.0f, -1.0f, 0.0f, 0.0f, texCoordRange.minV };
        vertices[1] = {  3.0f, -1.0f, 0.0f, 2.0f, texCoordRange.minV };
        vertices[2] = { -1.0f,  3.0f, 0.0f, 0.0f, texCoordRange.maxV };
    
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(viewID, program);
        return true;
    }
    
    void setupSMAAPostProcessViews(uint32_t width, uint32_t height) {
        const uint16_t viewWidth = static_cast<uint16_t>(std::min<uint32_t>(width, std::numeric_limits<uint16_t>::max()));
        const uint16_t viewHeight = static_cast<uint16_t>(std::min<uint32_t>(height, std::numeric_limits<uint16_t>::max()));
    
        bgfx::setViewMode(g_rendererSMAAEdgeViewID, bgfx::ViewMode::Sequential);
        bgfx::setViewRect(g_rendererSMAAEdgeViewID, 0, 0, viewWidth, viewHeight);
        bgfx::setViewClear(g_rendererSMAAEdgeViewID, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
        bgfx::setViewFrameBuffer(g_rendererSMAAEdgeViewID, g_SMAAState.edgesTarget.getFrameBuffer());
        bgfx::touch(g_rendererSMAAEdgeViewID);
    
        bgfx::setViewMode(g_rendererSMAABlendWeightViewID, bgfx::ViewMode::Sequential);
        bgfx::setViewRect(g_rendererSMAABlendWeightViewID, 0, 0, viewWidth, viewHeight);
        bgfx::setViewClear(g_rendererSMAABlendWeightViewID, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
        bgfx::setViewFrameBuffer(g_rendererSMAABlendWeightViewID, g_SMAAState.blendTarget.getFrameBuffer());
        bgfx::touch(g_rendererSMAABlendWeightViewID);
    
        bgfx::setViewMode(g_rendererSMAANeighborhoodViewID, bgfx::ViewMode::Sequential);
        bgfx::setViewRect(g_rendererSMAANeighborhoodViewID, 0, 0, viewWidth, viewHeight);
        bgfx::setViewClear(g_rendererSMAANeighborhoodViewID, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
        bgfx::setViewFrameBuffer(g_rendererSMAANeighborhoodViewID, BGFX_INVALID_HANDLE);
        bgfx::touch(g_rendererSMAANeighborhoodViewID);
    }
}

bool SMAA::isEnabled() {
    const std::string_view antiAliasing = OrkaConfig::ANTI_ALIASING.get();
    return antiAliasing == "SMAA" ||
           antiAliasing == "MSAA+SMAA" ||
           antiAliasing == "MSAAx2+SMAA" ||
           antiAliasing == "MSAAx4+SMAA";
}

void SMAA::destroyRenderTargets() {
    OrkaSMAAInternal::g_SMAAState.sceneTarget.destroy();
    OrkaSMAAInternal::g_SMAAState.edgesTarget.destroy();
    OrkaSMAAInternal::g_SMAAState.blendTarget.destroy();
}

void SMAA::destroyResources() {
    destroyRenderTargets();

    if (bgfx::isValid(OrkaSMAAInternal::g_SMAAState.areaTexture)) {
        bgfx::destroy(OrkaSMAAInternal::g_SMAAState.areaTexture);
        OrkaSMAAInternal::g_SMAAState.areaTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(OrkaSMAAInternal::g_SMAAState.searchTexture)) {
        bgfx::destroy(OrkaSMAAInternal::g_SMAAState.searchTexture);
        OrkaSMAAInternal::g_SMAAState.searchTexture = BGFX_INVALID_HANDLE;
    }

    bgfx::UniformHandle* handles[] = {
        &OrkaSMAAInternal::g_SMAAState.rtMetricsUniform,
        &OrkaSMAAInternal::g_SMAAState.colorSampler,
        &OrkaSMAAInternal::g_SMAAState.edgesSampler,
        &OrkaSMAAInternal::g_SMAAState.areaSampler,
        &OrkaSMAAInternal::g_SMAAState.searchSampler,
        &OrkaSMAAInternal::g_SMAAState.blendSampler
    };
    for (bgfx::UniformHandle* handle : handles) {
        if (bgfx::isValid(*handle)) {
            bgfx::destroy(*handle);
            *handle = BGFX_INVALID_HANDLE;
        }
    }
}

void SMAA::ensureResources(uint32_t width, uint32_t height) {
    OrkaSMAAInternal::ensureSMAAFullscreenLayout();
    OrkaSMAAInternal::ensureSMAAUniforms();
    OrkaSMAAInternal::ensureSMAALookupTextures();
    OrkaSMAAInternal::ensureSMAARenderTargets(width, height);
}

bgfx::FrameBufferHandle SMAA::getSceneFrameBuffer() {
    return OrkaSMAAInternal::g_SMAAState.sceneTarget.getFrameBuffer();
}

RenderTarget* SMAA::getSceneRenderTarget() {
    return &OrkaSMAAInternal::g_SMAAState.sceneTarget;
}

bool SMAA::runPostProcess(uint32_t width, uint32_t height, bgfx::ProgramHandle edgeProgram, bgfx::ProgramHandle blendWeightProgram, bgfx::ProgramHandle neighborhoodProgram) {
    ensureResources(width, height);
    if (!OrkaSMAAInternal::g_SMAAState.sceneTarget.isValid() ||
        !OrkaSMAAInternal::g_SMAAState.edgesTarget.isValid() ||
        !OrkaSMAAInternal::g_SMAAState.blendTarget.isValid() ||
        !bgfx::isValid(OrkaSMAAInternal::g_SMAAState.areaTexture) ||
        !bgfx::isValid(OrkaSMAAInternal::g_SMAAState.searchTexture)) {
        return false;
    }

    OrkaSMAAInternal::setupSMAAPostProcessViews(width, height);
    const float metrics[] = {
        1.0f / static_cast<float>(std::max<uint32_t>(width, 1)),
        1.0f / static_cast<float>(std::max<uint32_t>(height, 1)),
        static_cast<float>(std::max<uint32_t>(width, 1)),
        static_cast<float>(std::max<uint32_t>(height, 1))
    };

    const bgfx::TextureHandle sceneTexture = OrkaSMAAInternal::g_SMAAState.sceneTarget.getTexture(0);
    const bgfx::TextureHandle edgesTexture = OrkaSMAAInternal::g_SMAAState.edgesTarget.getTexture(0);
    const bgfx::TextureHandle blendTexture = OrkaSMAAInternal::g_SMAAState.blendTarget.getTexture(0);

    bgfx::setUniform(OrkaSMAAInternal::g_SMAAState.rtMetricsUniform, metrics);
    bgfx::setTexture(0, OrkaSMAAInternal::g_SMAAState.colorSampler, sceneTexture, OrkaSMAAInternal::g_SMAASamplerFlags);
    OrkaSMAAInternal::submitSMAAFullscreenTriangle(OrkaSMAAInternal::g_rendererSMAAEdgeViewID, edgeProgram);

    bgfx::setUniform(OrkaSMAAInternal::g_SMAAState.rtMetricsUniform, metrics);
    bgfx::setTexture(0, OrkaSMAAInternal::g_SMAAState.edgesSampler, edgesTexture, OrkaSMAAInternal::g_SMAASamplerFlags);
    bgfx::setTexture(1, OrkaSMAAInternal::g_SMAAState.areaSampler, OrkaSMAAInternal::g_SMAAState.areaTexture, OrkaSMAAInternal::g_SMAASamplerFlags);
    bgfx::setTexture(2, OrkaSMAAInternal::g_SMAAState.searchSampler, OrkaSMAAInternal::g_SMAAState.searchTexture, OrkaSMAAInternal::g_SMAASamplerFlags);
    OrkaSMAAInternal::submitSMAAFullscreenTriangle(OrkaSMAAInternal::g_rendererSMAABlendWeightViewID, blendWeightProgram);

    bgfx::setUniform(OrkaSMAAInternal::g_SMAAState.rtMetricsUniform, metrics);
    bgfx::setTexture(0, OrkaSMAAInternal::g_SMAAState.colorSampler, sceneTexture, OrkaSMAAInternal::g_SMAASamplerFlags);
    bgfx::setTexture(1, OrkaSMAAInternal::g_SMAAState.blendSampler, blendTexture, OrkaSMAAInternal::g_SMAASamplerFlags);
    OrkaSMAAInternal::submitSMAAFullscreenTriangle(OrkaSMAAInternal::g_rendererSMAANeighborhoodViewID, neighborhoodProgram);
    return true;
}

