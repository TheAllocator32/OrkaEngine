/*
 * File: RenderTarget.cpp
 *
 * Render target primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "RenderTarget.hpp"

#include <algorithm>
#include <limits>
#include <utility>

RenderTarget::~RenderTarget() {
    destroy();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept {
    moveFrom(other);
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
    if (this != &other) {
        destroy();
        moveFrom(other);
    }
    return *this;
}

bool RenderTarget::create(uint32_t width, uint32_t height, bgfx::TextureFormat::Enum colorFormat, uint64_t flags) {
    return create(width, height, { RenderTargetAttachment{ colorFormat, flags } });
}

bool RenderTarget::create(uint32_t width, uint32_t height, std::initializer_list<RenderTargetAttachment> attachments) {
    return create(width, height, std::vector<RenderTargetAttachment>(attachments));
}

bool RenderTarget::create(uint32_t width, uint32_t height, const std::vector<RenderTargetAttachment>& attachments) {
    destroy();
    if (attachments.empty() || attachments.size() > UINT8_MAX) {
        return false;
    }

    width = std::max<uint32_t>(width, 1);
    height = std::max<uint32_t>(height, 1);
    const uint16_t textureWidth = static_cast<uint16_t>(std::min<uint32_t>(width, std::numeric_limits<uint16_t>::max()));
    const uint16_t textureHeight = static_cast<uint16_t>(std::min<uint32_t>(height, std::numeric_limits<uint16_t>::max()));

    std::vector<bgfx::TextureHandle> textures;
    textures.reserve(attachments.size());
    for (const RenderTargetAttachment& attachment : attachments) {
        bgfx::TextureHandle texture = bgfx::createTexture2D(
            textureWidth,
            textureHeight,
            false,
            1,
            attachment.format,
            attachment.flags
        );
        if (!bgfx::isValid(texture)) {
            for (bgfx::TextureHandle createdTexture : textures) {
                bgfx::destroy(createdTexture);
            }
            return false;
        }
        textures.push_back(texture);
    }

    bgfx::FrameBufferHandle frameBuffer = bgfx::createFrameBuffer(
        static_cast<uint8_t>(textures.size()),
        textures.data(),
        true
    );
    if (!bgfx::isValid(frameBuffer)) {
        for (bgfx::TextureHandle texture : textures) {
            bgfx::destroy(texture);
        }
        return false;
    }

    m_frameBuffer = frameBuffer;
    m_width = width;
    m_height = height;
    m_attachments = attachments;
    return true;
}

bool RenderTarget::resize(uint32_t width, uint32_t height) {
    return create(width, height, m_attachments);
}

bool RenderTarget::ensure(uint32_t width, uint32_t height, bgfx::TextureFormat::Enum colorFormat, uint64_t flags) {
    return ensure(width, height, { RenderTargetAttachment{ colorFormat, flags } });
}

bool RenderTarget::ensure(uint32_t width, uint32_t height, std::initializer_list<RenderTargetAttachment> attachments) {
    return ensure(width, height, std::vector<RenderTargetAttachment>(attachments));
}

bool RenderTarget::ensure(uint32_t width, uint32_t height, const std::vector<RenderTargetAttachment>& attachments) {
    width = std::max<uint32_t>(width, 1);
    height = std::max<uint32_t>(height, 1);
    if (isValid() && m_width == width && m_height == height && m_attachments == attachments) {
        return true;
    }
    return create(width, height, attachments);
}

void RenderTarget::destroy() {
    if (bgfx::isValid(m_frameBuffer)) {
        bgfx::destroy(m_frameBuffer);
        m_frameBuffer = BGFX_INVALID_HANDLE;
    }
    m_width = 0;
    m_height = 0;
}

bgfx::TextureHandle RenderTarget::getTexture(uint8_t attachmentIndex) const {
    if (!isValid()) {
        return BGFX_INVALID_HANDLE;
    }
    return bgfx::getTexture(m_frameBuffer, attachmentIndex);
}

void RenderTarget::moveFrom(RenderTarget& other) {
    m_frameBuffer = other.m_frameBuffer;
    m_width = other.m_width;
    m_height = other.m_height;
    m_attachments = std::move(other.m_attachments);

    other.m_frameBuffer = BGFX_INVALID_HANDLE;
    other.m_width = 0;
    other.m_height = 0;
}

