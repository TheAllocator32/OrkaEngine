/*
 * File: ImageLoader.cpp
 *
 * Loads Image files (PNG/JPEG/EXR...)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "FileLoader.hpp"
#include "../Configurator.hpp"
#include "../essentials/Logger.hpp"
#include "../essentials/Timing.hpp"
#include "ext_stb_image.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <mutex>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

struct FileTextureCache {
    std::mutex mutex;
    std::unordered_map<std::string, std::weak_ptr<Texture::Storage>> entries;
};

static FileTextureCache& fileTextureCache() {
    static auto* cache = new FileTextureCache();
    return *cache;
}

static std::string canonicalTexturePath(std::string_view filepath) {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::weakly_canonical(filepath, ec);
    if (!ec) return path.string();

    ec.clear();
    path = std::filesystem::absolute(filepath, ec);
    if (!ec) return path.lexically_normal().string();
    return std::filesystem::path(filepath).lexically_normal().string();
}

static uint64_t resolveTextureFilteringFlags() {
    const std::string_view filtering = OrkaConfig::TEXTURE_FILTERING.get();
    if (filtering == "nearest") return BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;
    if (filtering == "anisotropic") return BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC;
    return BGFX_SAMPLER_NONE; // Linear is bgfx default when *_POINT flags are not set.
}

static void logLargeSceneNodeTexture(std::string_view filepath, int width, int height) {
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixels < 4096ull * 4096ull) return;

    const std::string filename = std::filesystem::path(filepath).filename().string();
    const std::size_t commentIndex = std::hash<std::string>{}(filename) % 3;
    std::string message = "Loading large texture '" + filename + "' (" +
        std::to_string(width) + "x" + std::to_string(height) + "). ";

    if (pixels >= 16384ull * 16384ull) {
        static constexpr const char* comments[] = {
            "This texture can probably see individual atoms. Loading may take quite a while.",
            "A texture of cartographic ambition has arrived. The GPU requests patience.",
            "At this resolution we may accidentally discover new continents. Please stand by."
        };
        message += comments[commentIndex];
    } else if (pixels >= 8192ull * 8192ull) {
        static constexpr const char* comments[] = {
            "8K: because apparently every pore needed its own postcode. This may take a while.",
            "That is enough pixels to wallpaper a small castle. Loading may take a moment.",
            "The GPU has located the 8K texture and is taking a deep breath."
        };
        message += comments[commentIndex];
    } else {
        static constexpr const char* comments[] = {
            "4K is a lot of pixels to herd; loading may take a moment.",
            "A crisp little mountain of pixels is heading to the GPU.",
            "Every texel has shown up personally. Give them a moment to find their seats."
        };
        message += comments[commentIndex];
    }

    Logger::log(LET_WARN, "Orka, ModelSceneNode", message);
}

bool FileLoader::buildMipChain(const uint8_t* pixels, int width, int height, bool generateMips, std::vector<uint8_t>& out) {
    if (pixels == nullptr || width <= 0 || height <= 0) return false;

    const size_t baseSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    out.assign(pixels, pixels + baseSize);
    if (!generateMips) return true;

    std::vector<uint8_t> previous(pixels, pixels + baseSize);
    int previousWidth = width;
    int previousHeight = height;

    while (previousWidth > 1 || previousHeight > 1) {
        const int mipWidth = std::max(1, previousWidth / 2);
        const int mipHeight = std::max(1, previousHeight / 2);
        std::vector<uint8_t> mip(static_cast<size_t>(mipWidth) * static_cast<size_t>(mipHeight) * 4u);

        for (int y = 0; y < mipHeight; ++y) {
            const int yStart = y * previousHeight / mipHeight;
            const int yEnd = (y + 1) * previousHeight / mipHeight;
            for (int x = 0; x < mipWidth; ++x) {
                const int xStart = x * previousWidth / mipWidth;
                const int xEnd = (x + 1) * previousWidth / mipWidth;
                const int sampleCount = (xEnd - xStart) * (yEnd - yStart);
                uint32_t sum[4] = { 0, 0, 0, 0 };
                for (int sourceY = yStart; sourceY < yEnd; ++sourceY) {
                    for (int sourceX = xStart; sourceX < xEnd; ++sourceX) {
                        const uint8_t* source = previous.data() + (static_cast<size_t>(sourceY) * previousWidth + sourceX) * 4u;
                        for (int channel = 0; channel < 4; ++channel) sum[channel] += source[channel];
                    }
                }
                uint8_t* destination = mip.data() + (static_cast<size_t>(y) * mipWidth + x) * 4u;
                for (int channel = 0; channel < 4; ++channel) destination[channel] = static_cast<uint8_t>(sum[channel] / sampleCount);
            }
        }

        out.insert(out.end(), mip.begin(), mip.end());
        previous = std::move(mip);
        previousWidth = mipWidth;
        previousHeight = mipHeight;
    }

    return out.size() <= std::numeric_limits<uint32_t>::max();
}

Texture FileLoader::loadImage(std::string_view filepath, bool generateMips, bool premultiplyAlpha, bool logLargeTexture) {
    Texture outTexture;
    if (filepath.empty()) {
        Logger::log(LET_WARN, "Orka, ImageLoader", "Refused to load image with empty path.");
        return outTexture;
    }
    const std::string filepathString = canonicalTexturePath(filepath);
    const uint64_t flags = resolveTextureFilteringFlags();
    const std::string cacheKey = filepathString + "|mips=" + (generateMips ? "1" : "0") +
        "|premultiply=" + (premultiplyAlpha ? "1" : "0") + "|flags=" + std::to_string(flags);
    {
        FileTextureCache& cache = fileTextureCache();
        std::lock_guard<std::mutex> lock(cache.mutex);
        auto it = cache.entries.find(cacheKey);
        if (it != cache.entries.end()) {
            if (std::shared_ptr<Texture::Storage> storage = it->second.lock()) {
                outTexture.m_storage = std::move(storage);
                return outTexture;
            }
            cache.entries.erase(it);
        }
    }

    int width = 0;
    int height = 0;
    int channelsInFile = 0;
    if (logLargeTexture && stbi_info(filepathString.c_str(), &width, &height, &channelsInFile)) {
        // Announce expensive textures before decoding them, while the delay still has an alibi.
        logLargeSceneNodeTexture(filepathString, width, height);
    }
    width = 0;
    height = 0;
    channelsInFile = 0;
    stbi_uc* pixels = stbi_load(filepathString.c_str(), &width, &height, &channelsInFile, 4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        Logger::log(LET_WARN, "Orka, ImageLoader", "Could not load image '" + filepathString + "'.");
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return outTexture;
    }

    if (premultiplyAlpha) {
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        for (size_t i = 0; i < pixelCount; ++i) {
            uint8_t* px = reinterpret_cast<uint8_t*>(pixels) + (i * 4);
            const uint8_t alpha = px[3];
            px[0] = static_cast<uint8_t>((static_cast<uint16_t>(px[0]) * alpha + 127u) / 255u);
            px[1] = static_cast<uint8_t>((static_cast<uint16_t>(px[1]) * alpha + 127u) / 255u);
            px[2] = static_cast<uint8_t>((static_cast<uint16_t>(px[2]) * alpha + 127u) / 255u);
        }
    }

    std::vector<uint8_t> pixelCopy;
    const bool hasMips = generateMips;
    if (!buildMipChain(pixels, width, height, hasMips, pixelCopy)) {
        Logger::log(LET_WARN, "Orka, ImageLoader", "Image '" + filepathString + "' is too large to upload.");
        stbi_image_free(pixels);
        return outTexture;
    }
    stbi_image_free(pixels);

    const uint32_t sizeBytes = static_cast<uint32_t>(pixelCopy.size());
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    Scheduler::runOnMainThreadBlocking([&handle, width, height, hasMips, flags, &pixelCopy, sizeBytes]() {
        const bgfx::Memory* mem = bgfx::copy(pixelCopy.data(), sizeBytes);
        handle = bgfx::createTexture2D(
            static_cast<uint16_t>(width),
            static_cast<uint16_t>(height),
            hasMips,
            1,
            bgfx::TextureFormat::RGBA8,
            flags,
            mem
        );
    });

    if (!bgfx::isValid(handle)) {
        Logger::log(LET_WARN, "Orka, ImageLoader", "Failed to upload image '" + filepathString + "' to bgfx texture.");
        return outTexture;
    }

    outTexture = handle;
    outTexture.setWidth(static_cast<uint16_t>(width));
    outTexture.setHeight(static_cast<uint16_t>(height));
    outTexture.setFormat(bgfx::TextureFormat::RGBA8);
    outTexture.setFlags(flags);
    {
        FileTextureCache& cache = fileTextureCache();
        std::lock_guard<std::mutex> lock(cache.mutex);
        auto it = cache.entries.find(cacheKey);
        if (it != cache.entries.end()) {
            if (std::shared_ptr<Texture::Storage> storage = it->second.lock()) {
                outTexture.m_storage = std::move(storage);
                return outTexture;
            }
        }
        cache.entries[cacheKey] = outTexture.m_storage;
    }
    return outTexture;
}
