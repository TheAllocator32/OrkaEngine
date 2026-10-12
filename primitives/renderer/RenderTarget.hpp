/*
 * File: RenderTarget.hpp
 *
 * A framebuffer-backed texture target.
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
#include <cstdint>
#include <initializer_list>
#include <vector>

struct RenderTargetAttachment {
    bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8;
    uint64_t flags = BGFX_TEXTURE_RT;

    inline bool operator==(const RenderTargetAttachment& other) const {
        return format == other.format && flags == other.flags;
    }
};

class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    // Creates a single-color-attachment render target.
    bool create(uint32_t width, uint32_t height, bgfx::TextureFormat::Enum colorFormat, uint64_t flags = BGFX_TEXTURE_RT);

    // Creates a render target with the given attachment list.
    bool create(uint32_t width, uint32_t height, std::initializer_list<RenderTargetAttachment> attachments);

    // Creates a render target with the given attachment list.
    bool create(uint32_t width, uint32_t height, const std::vector<RenderTargetAttachment>& attachments);

    // Recreates the render target at a new size using its existing attachments.
    bool resize(uint32_t width, uint32_t height);

    // Creates or reuses a single-color-attachment render target.
    bool ensure(uint32_t width, uint32_t height, bgfx::TextureFormat::Enum colorFormat, uint64_t flags = BGFX_TEXTURE_RT);

    // Creates or reuses a render target with the given attachment list.
    bool ensure(uint32_t width, uint32_t height, std::initializer_list<RenderTargetAttachment> attachments);

    // Creates or reuses a render target with the given attachment list.
    bool ensure(uint32_t width, uint32_t height, const std::vector<RenderTargetAttachment>& attachments);

    // Destroys the framebuffer and owned attachment textures.
    void destroy();

    // Returns true when the underlying bgfx framebuffer is valid.
    inline bool isValid() const { return bgfx::isValid(m_frameBuffer); }
    inline explicit operator bool() const { return isValid(); }

    // Returns the underlying bgfx framebuffer handle.
    inline bgfx::FrameBufferHandle getFrameBuffer() const { return m_frameBuffer; }

    // Returns a texture handle for one framebuffer attachment.
    bgfx::TextureHandle getTexture(uint8_t attachmentIndex = 0) const;

    // Returns the requested render target width.
    inline uint32_t getWidth() const { return m_width; }

    // Returns the requested render target height.
    inline uint32_t getHeight() const { return m_height; }

    // Returns the attachment descriptors used to create the render target.
    inline const std::vector<RenderTargetAttachment>& getAttachments() const { return m_attachments; }

private:
    // Transfers ownership from another render target.
    void moveFrom(RenderTarget& other);

    bgfx::FrameBufferHandle m_frameBuffer = BGFX_INVALID_HANDLE;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<RenderTargetAttachment> m_attachments;
};

