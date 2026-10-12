/*
 * File: Texture.hpp
 *
 * A simple Texture
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
#include <memory>

class FileLoader; // Stupid forward declaration!

// Dummy, Placeholder, not finished! But it kinda works
struct Texture {
    struct Storage {
        bgfx::TextureHandle textureID = BGFX_INVALID_HANDLE;
        uint16_t width = 0;
        uint16_t height = 0;
        bgfx::TextureFormat::Enum format = bgfx::TextureFormat::Unknown;
        uint64_t flags = BGFX_TEXTURE_NONE;
        ~Storage();
    };

    // Constructors
    Texture();
    explicit Texture(bgfx::TextureHandle h);
    Texture(const Texture&) = default;
    Texture& operator=(const Texture&) = default;

    // Move
    Texture(Texture&& other) noexcept = default;
    Texture& operator=(Texture&& other) noexcept = default;

    // Operators
    // This will DESTROY the current Texture!
    Texture& operator=(bgfx::TextureHandle id);
    inline operator bgfx::TextureHandle() const { return m_storage->textureID; }
    inline bool operator!() const { return !bgfx::isValid(m_storage->textureID); }
    inline explicit operator bool() const { return bgfx::isValid(m_storage->textureID); }

    // Destructor
    ~Texture() = default;
    void destroy();

    // Clones the texture wrapper. GPU data stays shared until one side replaces/destroys it.
    Texture* clone() const;

private:
    friend class FileLoader; // FileLoader can access private members if needed
    inline void setWidth(uint16_t width) { m_storage->width = width; }
    inline void setHeight(uint16_t height) { m_storage->height = height; }
    inline void setFormat(bgfx::TextureFormat::Enum format) { m_storage->format = format; }
    inline void setFlags(uint64_t flags) { m_storage->flags = flags; }
public:
    inline uint16_t getWidth() const { return m_storage->width; }
    inline uint16_t getHeight() const { return m_storage->height; }
    inline bgfx::TextureFormat::Enum getFormat() const { return m_storage->format; }
    inline uint64_t getFlags() const { return m_storage->flags; }
private:
    std::shared_ptr<Storage> m_storage;
};

