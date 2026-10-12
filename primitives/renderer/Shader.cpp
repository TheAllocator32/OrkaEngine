/*
 * File: Shader.cpp
 *
 * Shader primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Shader.hpp"
#include <algorithm>
#include <array>
#include <filesystem>

void Shader::setUniform(std::string_view name, ShaderUniformType value) {
    std::erase_if(m_uniforms,
        [&](const auto& u) { return u.first == name; });
    m_uniforms.emplace_back(name, value);
}

Shader Shader::fromFile(std::string_view pathWithoutFileExtension, ShaderFileLayout layout, RenderSystemType renderSystem) {
    const RenderSystemType resolvedRenderSystem = resolveRenderSystem(renderSystem);
    const std::string extensionPrefix = getFileBackendSuffix(resolvedRenderSystem);

    if (extensionPrefix.empty()) {
        Logger::log(LET_WARN, "Orka, Shader", "Could not resolve shader backend extension. Returning empty shader.");
        return Shader();
    }

    std::string basePath(pathWithoutFileExtension);
    std::filesystem::path check(basePath);
    if (check.has_extension()) {
        Logger::log(LET_WARN, "Orka, Shader", "Ooof. Really? I said no extensions please! '" + basePath + "' Proceeding anyway...");
    }

    const std::string singlePath = basePath + "." + extensionPrefix + ".shader";
    const std::string vertexPath = basePath + ".vert." + extensionPrefix + ".shader";
    const std::string fragmentPath = basePath + ".frag." + extensionPrefix + ".shader";
    const bool hasSplitStageFiles = fileExists(vertexPath) && fileExists(fragmentPath);

    if (layout == SFL_STAGE_FILES || (layout == SFL_AUTO && hasSplitStageFiles)) {
        return Shader(vertexPath, fragmentPath);
    }

    return Shader(singlePath);
}

Shader Shader::fromBuffers(ShaderBufferView vertexBuffer, ShaderBufferView fragmentBuffer) {
    Shader shader;
    shader.m_vertexSource = ShaderStageSource::binary(vertexBuffer);
    shader.m_fragmentSource = ShaderStageSource::binary(fragmentBuffer);
    return shader;
}

Shader Shader::fromStrings(std::string_view vertexCode, std::string_view fragmentCode) {
    Shader shader;
    shader.m_vertexSource = ShaderStageSource::source(vertexCode);
    shader.m_fragmentSource = ShaderStageSource::source(fragmentCode);
    return shader;
}

RenderSystemType Shader::resolveRenderSystem(RenderSystemType requestedRenderSystem) {
    if (requestedRenderSystem != RST_AUTO) return requestedRenderSystem;
    if (sm_activeRenderSystem != RST_AUTO && sm_activeRenderSystem != RST_NONE) return sm_activeRenderSystem;
    return getConfiguredRenderSystem();
}

std::string Shader::getFileBackendSuffix(RenderSystemType renderSystem) {
    switch (renderSystem) {
    case RST_VULKAN:
        return "vk";
    case RST_DIRECTX12:
        return "dx12";
    case RST_DIRECTX11:
        return "dx11";
    case RST_OPENGL4:
        return "gln";
    case RST_OPENGL2:
        return "glo";
    case RST_OPENGLES:
        return "gles";
    case RST_METAL:
        return "mt";
    case RST_WEBGL:
        return "web";
    case RST_AUTO:
        return getFileBackendSuffix(resolveRenderSystem(RST_AUTO));
    case RST_NONE:
        return "";
    }
    return "";
}

std::string Shader::getGeneratedBackendSuffix(RenderSystemType renderSystem) {
    switch (renderSystem) {
    case RST_VULKAN:
        return "vulkan";
    case RST_DIRECTX12:
        return "dx12";
    case RST_DIRECTX11:
        return "dx11";
    case RST_OPENGL4:
        return "GL";
    case RST_OPENGL2:
        return "GL_LEGACY";
    case RST_OPENGLES:
        return "GLES";
    case RST_METAL:
        return "metal";
    case RST_WEBGL:
        return "WEBGL";
    case RST_AUTO:
        return getGeneratedBackendSuffix(resolveRenderSystem(RST_AUTO));
    case RST_NONE:
        return "";
    }
    return "";
}

ShaderBufferView Shader::selectBufferForRenderSystem(RenderSystemType renderSystem, ShaderBufferView gl, ShaderBufferView glLegacy, ShaderBufferView gles, ShaderBufferView webgl, ShaderBufferView vulkan, ShaderBufferView dx11, ShaderBufferView dx12, ShaderBufferView metal) {
    ShaderBufferView preferred;
    switch (renderSystem) {
    case RST_VULKAN:
        preferred = vulkan;
        break;
    case RST_DIRECTX12:
        preferred = dx12;
        break;
    case RST_DIRECTX11:
        preferred = dx11;
        break;
    case RST_OPENGL4:
        preferred = gl;
        break;
    case RST_OPENGL2:
        preferred = glLegacy;
        break;
    case RST_OPENGLES:
        preferred = gles;
        break;
    case RST_WEBGL:
        preferred = webgl;
        break;
    case RST_METAL:
        preferred = metal;
        break;
    case RST_NONE:
    case RST_AUTO:
        break;
    }

    if (!preferred.empty()) return preferred;

    const std::array<ShaderBufferView, 8> fallback = {
        gl,
        glLegacy,
        gles,
        webgl,
        vulkan,
        dx11,
        dx12,
        metal
    };
    for (const ShaderBufferView view : fallback) {
        if (!view.empty()) return view;
    }

    return ShaderBufferView();
}

bool Shader::fileExists(std::string_view path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec) && !ec;
}

RenderSystemType Shader::getConfiguredRenderSystem() {
    const std::string_view configured = OrkaConfig::RENDERSYSTEM.get();
    if (configured == "vulkan") return RST_VULKAN;
    if (configured == "opengl") return RST_OPENGL4;
    if (configured == "opengles") return RST_OPENGLES;
    if (configured == "dx11") return RST_DIRECTX11;
    if (configured == "dx12") return RST_DIRECTX12;
    if (configured == "metal") return RST_METAL;
    if (configured == "webgl") return RST_WEBGL;
    return RST_NONE;
}

