/*
 * File: Texture.cpp
 *
 * Texture primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Texture.hpp"
#include <memory>
#include "../../essentials/Timing.hpp"

Texture::Storage::~Storage() {
    if (!bgfx::isValid(textureID)) {
        return;
    }

    const bgfx::TextureHandle handle = textureID;
    textureID = BGFX_INVALID_HANDLE;
    Scheduler::runOnMainThreadBlocking([handle]() {
        if (bgfx::isValid(handle)) {
            bgfx::destroy(handle);
        }
    });
}

Texture::Texture() : m_storage(std::make_shared<Storage>()) {}

Texture::Texture(bgfx::TextureHandle h) : m_storage(std::make_shared<Storage>()) {
    m_storage->textureID = h;
}

Texture& Texture::operator=(bgfx::TextureHandle id) {
    destroy();
    m_storage->textureID = id;
    return *this;
}

void Texture::destroy() {
    if (!bgfx::isValid(m_storage->textureID)) {
        return;
    }
    m_storage = std::make_shared<Storage>();
}

Texture* Texture::clone() const {
    if (!bgfx::isValid(m_storage->textureID)) {
        return nullptr;
    }
    return new Texture(*this);
}

