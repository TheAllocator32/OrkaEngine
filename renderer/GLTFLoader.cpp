/*
 * File: GLTFLoader.cpp
 *
 * Loads GLB/GLTF and converts it to Orka Meshes
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "FileLoader.hpp"
#include "ext_tiny_gltf_v3.h"
#include "ext_stb_image.h"
#include "../Configurator.hpp"
#include "../primitives/3d/Geometry.hpp"
#include "../essentials/Logger.hpp"
#include "../essentials/Timing.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#ifndef ORKA_DISABLE_MESHOPT
#define ORKA_ENABLE_MESHOPT
#endif
#ifdef ORKA_ENABLE_MESHOPT
#include "meshoptimizer.h"
#endif

struct AccessView {
    const unsigned char* data = nullptr;
    size_t stride = 0;
    size_t count = 0;
    int componentType = 0;
    int type = 0;
    bool normalized = false;
};

struct MeshoptBufferView {
    std::vector<unsigned char> decodedData;
    size_t byteStride = 0;
};

static const tg3_extension* findExtension(const tg3_extras_ext& ext, const char* name) {
    for (uint32_t i = 0; i < ext.extensions_count; ++i) {
        if (tg3_str_equals_cstr(ext.extensions[i].name, name)) return &ext.extensions[i];
    }
    return nullptr;
}

static const tg3_value* findObjectValue(const tg3_value& object, const char* name) {
    if (object.type != TG3_VALUE_OBJECT) return nullptr;
    for (uint32_t i = 0; i < object.object_count; ++i) {
        if (tg3_str_equals_cstr(object.object_data[i].key, name)) return &object.object_data[i].value;
    }
    return nullptr;
}

static bool getNonNegativeInteger(const tg3_value* value, uint64_t& out) {
    if (!value || value->type != TG3_VALUE_INT || value->int_val < 0) return false;
    out = static_cast<uint64_t>(value->int_val);
    return true;
}

static bool meshoptStringEquals(const tg3_value* value, const char* expected) {
    return value && value->type == TG3_VALUE_STRING && tg3_str_equals_cstr(value->string_val, expected);
}

#ifdef ORKA_ENABLE_MESHOPT
static bool decodeMeshoptBufferViews(const tg3_model& model, std::unordered_map<int, MeshoptBufferView>& decodedBufferViews, std::string& error) {
    for (uint32_t bufferViewIndex = 0; bufferViewIndex < model.buffer_views_count; ++bufferViewIndex) {
        const tg3_buffer_view& bufferView = model.buffer_views[bufferViewIndex];
        const tg3_extension* khrExtension = findExtension(bufferView.ext, "KHR_meshopt_compression");
        const tg3_extension* extExtension = findExtension(bufferView.ext, "EXT_meshopt_compression");
        if (khrExtension && extExtension) {
            error = "bufferView uses both KHR_meshopt_compression and EXT_meshopt_compression";
            return false;
        }

        const tg3_extension* extension = khrExtension ? khrExtension : extExtension;
        const bool isKhrMeshoptCompression = khrExtension != nullptr;
        if (!extension) continue;

        uint64_t bufferIndex = 0;
        uint64_t byteOffset = 0;
        uint64_t byteLength = 0;
        uint64_t byteStride = 0;
        uint64_t count = 0;
        const tg3_value* byteOffsetValue = findObjectValue(extension->value, "byteOffset");
        if (!getNonNegativeInteger(findObjectValue(extension->value, "buffer"), bufferIndex) ||
            !getNonNegativeInteger(findObjectValue(extension->value, "byteLength"), byteLength) ||
            !getNonNegativeInteger(findObjectValue(extension->value, "byteStride"), byteStride) ||
            !getNonNegativeInteger(findObjectValue(extension->value, "count"), count) ||
            (byteOffsetValue && !getNonNegativeInteger(byteOffsetValue, byteOffset)) ||
            bufferIndex >= model.buffers_count || byteStride == 0 || byteStride > std::numeric_limits<size_t>::max() ||
            byteLength > std::numeric_limits<size_t>::max() || count > std::numeric_limits<size_t>::max() / static_cast<size_t>(byteStride)) {
            error = "meshopt compression has invalid bufferView metadata";
            return false;
        }

        const tg3_buffer& sourceBuffer = model.buffers[bufferIndex];
        if (!sourceBuffer.data.data || byteOffset > sourceBuffer.data.count || byteLength > sourceBuffer.data.count - byteOffset) {
            error = "meshopt compression reads outside its source buffer";
            return false;
        }

        const tg3_value* mode = findObjectValue(extension->value, "mode");
        const tg3_value* filter = findObjectValue(extension->value, "filter");
        const bool isAttributes = meshoptStringEquals(mode, "ATTRIBUTES");
        const bool isTriangles = meshoptStringEquals(mode, "TRIANGLES");
        const bool isIndices = meshoptStringEquals(mode, "INDICES");
        if (!isAttributes && !isTriangles && !isIndices) {
            error = "meshopt compression uses an unsupported mode";
            return false;
        }
        if ((isAttributes && (byteStride < 4 || byteStride > 256 || byteStride % 4 != 0)) ||
            (!isAttributes && (byteStride != 2 && byteStride != 4)) ||
            (isTriangles && count % 3 != 0) ||
            bufferView.byte_length != count * byteStride) {
            error = "meshopt compression has an invalid decoded bufferView layout";
            return false;
        }

        if (!filter || meshoptStringEquals(filter, "NONE")) {
        } else if (!isAttributes) {
            error = "meshopt compression filters require ATTRIBUTES mode";
            return false;
        } else if (meshoptStringEquals(filter, "OCTAHEDRAL") && byteStride != 4 && byteStride != 8) {
            error = "meshopt OCTAHEDRAL filter requires a 4 or 8 byte stride";
            return false;
        } else if (meshoptStringEquals(filter, "QUATERNION") && byteStride != 8) {
            error = "meshopt QUATERNION filter requires an 8 byte stride";
            return false;
        } else if (meshoptStringEquals(filter, "EXPONENTIAL") && byteStride % 4 != 0) {
            error = "meshopt EXPONENTIAL filter requires a stride divisible by 4";
            return false;
        } else if (meshoptStringEquals(filter, "COLOR") && (!isKhrMeshoptCompression || (byteStride != 4 && byteStride != 8))) {
            error = "meshopt COLOR filter requires KHR_meshopt_compression and a 4 or 8 byte stride";
            return false;
        } else if (!meshoptStringEquals(filter, "OCTAHEDRAL") && !meshoptStringEquals(filter, "QUATERNION") &&
                   !meshoptStringEquals(filter, "EXPONENTIAL") && !meshoptStringEquals(filter, "COLOR")) {
            error = "meshopt compression uses an unsupported filter";
            return false;
        }

        MeshoptBufferView decoded;
        decoded.byteStride = static_cast<size_t>(byteStride);
        decoded.decodedData.resize(static_cast<size_t>(count * byteStride));
        const unsigned char* source = sourceBuffer.data.data + byteOffset;
        int decodeResult = -1;
        if (isAttributes) {
            // meshoptimizer recognizes both the EXT-compatible v0 and KHR v1 vertex bitstreams.
            decodeResult = meshopt_decodeVertexBuffer(decoded.decodedData.data(), static_cast<size_t>(count), decoded.byteStride, source, static_cast<size_t>(byteLength));
        } else if (isTriangles) {
            decodeResult = meshopt_decodeIndexBuffer(decoded.decodedData.data(), static_cast<size_t>(count), decoded.byteStride, source, static_cast<size_t>(byteLength));
        } else if (isIndices) {
            decodeResult = meshopt_decodeIndexSequence(decoded.decodedData.data(), static_cast<size_t>(count), decoded.byteStride, source, static_cast<size_t>(byteLength));
        }
        if (decodeResult != 0) {
            error = "meshopt compression failed to decode a bufferView";
            return false;
        }

        if (!filter || meshoptStringEquals(filter, "NONE")) {
        } else if (meshoptStringEquals(filter, "OCTAHEDRAL")) {
            meshopt_decodeFilterOct(decoded.decodedData.data(), static_cast<size_t>(count), decoded.byteStride);
        } else if (meshoptStringEquals(filter, "QUATERNION")) {
            meshopt_decodeFilterQuat(decoded.decodedData.data(), static_cast<size_t>(count), decoded.byteStride);
        } else if (meshoptStringEquals(filter, "EXPONENTIAL")) {
            meshopt_decodeFilterExp(decoded.decodedData.data(), static_cast<size_t>(count), decoded.byteStride);
        } else if (meshoptStringEquals(filter, "COLOR") && isKhrMeshoptCompression) {
            meshopt_decodeFilterColor(decoded.decodedData.data(), static_cast<size_t>(count), decoded.byteStride);
        }

        decodedBufferViews.emplace(static_cast<int>(bufferViewIndex), std::move(decoded));
    }
    return true;
}
#endif

AccessView accessOf(const tg3_model& m, const tg3_accessor& acc, const std::unordered_map<int, MeshoptBufferView>& decodedBufferViews) {
    if (acc.buffer_view < 0 || acc.buffer_view >= static_cast<int32_t>(m.buffer_views_count)) {
        throw std::runtime_error("accessor has no supported bufferView");
    }

    const tg3_buffer_view& bv = m.buffer_views[acc.buffer_view];
    if (auto decodedIt = decodedBufferViews.find(acc.buffer_view); decodedIt != decodedBufferViews.end()) {
        const MeshoptBufferView& decoded = decodedIt->second;
        const uint64_t byteOffset = acc.byte_offset;
        const int32_t componentSize = tg3_component_size(acc.component_type);
        const int32_t componentCount = tg3_num_components(acc.type);
        if (componentSize <= 0 || componentCount <= 0) throw std::runtime_error("accessor has invalid component layout");

        const uint64_t elementSize = static_cast<uint64_t>(componentSize) * static_cast<uint64_t>(componentCount);
        const uint64_t lastByte = byteOffset + ((acc.count > 0) ? ((acc.count - 1) * decoded.byteStride + elementSize) : 0);
        if (byteOffset > decoded.decodedData.size() || lastByte > decoded.decodedData.size()) {
            throw std::runtime_error("accessor reads outside decoded meshopt bufferView");
        }

        return { decoded.decodedData.data() + byteOffset, decoded.byteStride, acc.count, acc.component_type, acc.type, acc.normalized != 0 };
    }

    if (bv.buffer < 0 || bv.buffer >= static_cast<int32_t>(m.buffers_count)) {
        throw std::runtime_error("accessor references invalid buffer");
    }

    const tg3_buffer& b = m.buffers[bv.buffer];
    if (!b.data.data || b.data.count == 0) {
        throw std::runtime_error("accessor references empty buffer data");
    }

    const uint64_t byteOffset = bv.byte_offset + acc.byte_offset;
    const int32_t stride = tg3_accessor_byte_stride(&acc, &bv);
    if (stride <= 0) {
        throw std::runtime_error("accessor has invalid byte stride");
    }

    const int32_t componentSize = tg3_component_size(acc.component_type);
    const int32_t componentCount = tg3_num_components(acc.type);
    if (componentSize <= 0 || componentCount <= 0) {
        throw std::runtime_error("accessor has invalid component layout");
    }

    const uint64_t elementSize = static_cast<uint64_t>(componentSize) * static_cast<uint64_t>(componentCount);
    const uint64_t lastByte = byteOffset + ((acc.count > 0) ? ((acc.count - 1) * static_cast<uint64_t>(stride) + elementSize) : 0);
    if (byteOffset > b.data.count || lastByte > b.data.count) {
        throw std::runtime_error("accessor reads outside buffer bounds");
    }

    AccessView a;
    a.data = b.data.data + byteOffset;
    a.stride = static_cast<size_t>(stride);
    a.count = acc.count;
    a.componentType = acc.component_type;
    a.type = acc.type;
    a.normalized = acc.normalized;
    return a;
}

float readAsFloat(const unsigned char* p, int ct, bool norm) {
    switch (ct) {
    case TG3_COMPONENT_TYPE_FLOAT: return *reinterpret_cast<const float*>(p);
    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:  return norm ? (*p / 255.0f) : float(*p);
    case TG3_COMPONENT_TYPE_BYTE:           return norm ? std::max(int(*reinterpret_cast<const int8_t*>(p)) / 127.0f, -1.0f) : float(*reinterpret_cast<const int8_t*>(p));
    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: return norm ? (*reinterpret_cast<const uint16_t*>(p) / 65535.0f) : float(*reinterpret_cast<const uint16_t*>(p));
    case TG3_COMPONENT_TYPE_SHORT:          return norm ? std::max(int(*reinterpret_cast<const int16_t*>(p)) / 32767.0f, -1.0f) : float(*reinterpret_cast<const int16_t*>(p));
    case TG3_COMPONENT_TYPE_UNSIGNED_INT:   return float(*reinterpret_cast<const uint32_t*>(p));
    case TG3_COMPONENT_TYPE_INT:            return float(*reinterpret_cast<const int32_t*>(p));
    default: return 0.0f;
    }
}

template<int N>
void readVecN(const AccessView& a, size_t i, float out[N]) {
    const unsigned char* p = a.data + i * a.stride;
    const int cs = tg3_component_size(a.componentType);
    for (int k = 0; k < N; ++k) out[k] = readAsFloat(p + k * cs, a.componentType, a.normalized);
}

inline uint16_t u16FromNormFloat(float w) {
    w = std::clamp(w, 0.0f, 1.0f);
    return static_cast<uint16_t>(std::lround(w * 65535.0f));
}
inline uint16_t u16FromU8(uint8_t v) { return uint16_t(v) * 257u; }
inline int u8FromNormFloat(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return int(std::lround(c * 255.0f));
}

static uint64_t resolveTextureFilteringFlags() {
    const std::string_view filtering = OrkaConfig::TEXTURE_FILTERING.get();
    if (filtering == "nearest") return BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;
    if (filtering == "anisotropic") return BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC;
    return BGFX_SAMPLER_NONE; // Linear is bgfx default when *_POINT flags are not set.
}

struct ConvertedTinyImage {
    std::vector<uint8_t> pixels;
    bgfx::TextureFormat::Enum format = bgfx::TextureFormat::Unknown;
};

struct EncodedImageView {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

struct GLTFTextureCache {
    std::mutex mutex;
    std::unordered_map<std::string, std::weak_ptr<Texture>> entries;
};

static GLTFTextureCache& gltfTextureCache() {
    static auto* cache = new GLTFTextureCache();
    return *cache;
}

static std::string canonicalAssetPath(const std::filesystem::path& filepath) {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::weakly_canonical(filepath, ec);
    if (!ec) return path.string();
    ec.clear();
    path = std::filesystem::absolute(filepath, ec);
    if (!ec) return path.lexically_normal().string();
    return filepath.lexically_normal().string();
}

static std::shared_ptr<Texture> findCachedGLTFTexture(const std::string& key) {
    GLTFTextureCache& cache = gltfTextureCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    auto it = cache.entries.find(key);
    if (it == cache.entries.end()) return {};
    std::shared_ptr<Texture> texture = it->second.lock();
    if (!texture) cache.entries.erase(it);
    return texture;
}

static std::shared_ptr<Texture> cacheGLTFTexture(const std::string& key, std::shared_ptr<Texture> texture) {
    if (!texture) return {};
    GLTFTextureCache& cache = gltfTextureCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    auto it = cache.entries.find(key);
    if (it != cache.entries.end()) {
        if (std::shared_ptr<Texture> existing = it->second.lock()) return existing;
    }
    cache.entries[key] = texture;
    return texture;
}

static std::string tg3String(tg3_str str) {
    return str.data ? std::string(str.data, str.len) : std::string();
}

static void logLargeSceneNodeTexture(std::string_view textureName, int width, int height) {
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixels < 4096ull * 4096ull) return;

    std::string filename = std::filesystem::path(textureName).filename().string();
    if (filename.empty()) filename = "embedded texture";
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

static bool tg3StringEquals(tg3_str str, const char* value) {
    return tg3_str_equals_cstr(str, value) != 0;
}

static void logLargeSceneNodeGeometry(std::string_view filepath, const tg3_model& model) {
    std::uint64_t vertices = 0;
    std::uint64_t triangles = 0;

    for (uint32_t meshIndex = 0; meshIndex < model.meshes_count; ++meshIndex) {
        const tg3_mesh& mesh = model.meshes[meshIndex];
        for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex) {
            const tg3_primitive& primitive = mesh.primitives[primitiveIndex];
            std::uint64_t primitiveVertices = 0;
            for (uint32_t attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex) {
                const tg3_str_int_pair& attribute = primitive.attributes[attributeIndex];
                if (!tg3StringEquals(attribute.key, "POSITION")) continue;
                if (attribute.value >= 0 && attribute.value < static_cast<int32_t>(model.accessors_count)) {
                    primitiveVertices = model.accessors[attribute.value].count;
                    vertices += primitiveVertices;
                }
                break;
            }
            std::uint64_t primitiveIndices = primitiveVertices;
            if (primitive.indices >= 0 && primitive.indices < static_cast<int32_t>(model.accessors_count)) {
                primitiveIndices = model.accessors[primitive.indices].count;
            }
            if (primitive.mode == -1 || primitive.mode == 4) triangles += primitiveIndices / 3;
        }
    }

    if (vertices < 500000) return;
    const std::string filename = std::filesystem::path(filepath).filename().string();
    const std::size_t commentIndex = std::hash<std::string>{}(filename) % 3;
    std::string message = "Loading large model geometry from '" + filename + "' (" +
        std::to_string(vertices) + " vertices, " + std::to_string(triangles) + " triangles). ";

    if (vertices >= 10000000) {
        static constexpr const char* comments[] = {
            "That is not a mesh, that is a digital mountain range. Loading may take a while.",
            "The polygon buffet was apparently all-you-can-eat. Please stand by.",
            "This model brought enough vertices to start its own city."
        };
        message += comments[commentIndex];
    } else if (vertices >= 2000000) {
        static constexpr const char* comments[] = {
            "A very well-fed mesh is approaching; the loader has fetched a forklift.",
            "That is serious geometry. The vertices are forming an orderly queue.",
            "This model has some polygons on its bones. Loading may take a moment."
        };
        message += comments[commentIndex];
    } else {
        static constexpr const char* comments[] = {
            "A moderately chunky mesh is being assembled.",
            "Not terrifying, but the vertices may need a moment to find their seats.",
            "A substantial polygon snack is on the way."
        };
        message += comments[commentIndex];
    }

    Logger::log(LET_WARN, "Orka, ModelSceneNode", message);
}

static bool tg3StringListContains(const tg3_str* strings, uint32_t count, const char* value) {
    if (!strings || !value) return false;
    for (uint32_t i = 0; i < count; ++i) {
        if (tg3StringEquals(strings[i], value)) return true;
    }
    return false;
}

static std::string tg3ErrorSummary(const tg3_error_stack& errors) {
    std::string out;
    const uint32_t count = tg3_errors_count(&errors);
    for (uint32_t i = 0; i < count; ++i) {
        const tg3_error_entry* entry = tg3_errors_get(&errors, i);
        if (!entry || !entry->message) continue;
        if (!out.empty()) out += "\n\t";
        out += entry->message;
    }
    return out;
}

struct ScopedTg3Model {
    tg3_model model{};
    ~ScopedTg3Model() { tg3_model_free(&model); }
};

static bool convertTinyImageToRGBA(const tg3_image& image, ConvertedTinyImage& converted) {
    converted.pixels.clear();
    converted.format = bgfx::TextureFormat::Unknown;

    if (image.width <= 0 || image.height <= 0 || image.component <= 0 || image.image.count == 0) return false;

    const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    const int components = image.component;

    if (image.bits == 8) {
        converted.format = bgfx::TextureFormat::RGBA8;
        converted.pixels.resize(pixelCount * 4u);

        for (size_t i = 0; i < pixelCount; ++i) {
            const size_t srcOffset = i * static_cast<size_t>(components);
            const size_t dstOffset = i * 4u;
            const uint8_t c0 = image.image.data[srcOffset + 0];
            const uint8_t c1 = (components > 1) ? image.image.data[srcOffset + 1] : c0;
            const uint8_t c2 = (components > 2) ? image.image.data[srcOffset + 2] : c0;
            const uint8_t c3 = (components > 3) ? image.image.data[srcOffset + 3] : 255u;
            converted.pixels[dstOffset + 0] = c0;
            converted.pixels[dstOffset + 1] = c1;
            converted.pixels[dstOffset + 2] = c2;
            converted.pixels[dstOffset + 3] = c3;
        }

        return true;
    }

    if (image.bits == 16) {
        converted.format = bgfx::TextureFormat::RGBA16;
        converted.pixels.resize(pixelCount * 4u * sizeof(uint16_t));

        auto readU16 = [&](size_t componentIndex) -> uint16_t {
            uint16_t value = 0;
            const size_t byteOffset = componentIndex * sizeof(uint16_t);
            std::memcpy(&value, image.image.data + byteOffset, sizeof(uint16_t));
            return value;
        };

        auto writeU16 = [&](size_t componentIndex, uint16_t value) {
            const size_t byteOffset = componentIndex * sizeof(uint16_t);
            std::memcpy(converted.pixels.data() + byteOffset, &value, sizeof(uint16_t));
        };

        for (size_t i = 0; i < pixelCount; ++i) {
            const size_t srcOffset = i * static_cast<size_t>(components);
            const size_t dstOffset = i * 4u;
            const uint16_t c0 = readU16(srcOffset + 0);
            const uint16_t c1 = (components > 1) ? readU16(srcOffset + 1) : c0;
            const uint16_t c2 = (components > 2) ? readU16(srcOffset + 2) : c0;
            const uint16_t c3 = (components > 3) ? readU16(srcOffset + 3) : 65535u;
            writeU16(dstOffset + 0, c0);
            writeU16(dstOffset + 1, c1);
            writeU16(dstOffset + 2, c2);
            writeU16(dstOffset + 3, c3);
        }

        return true;
    }

    return false;
}

static int decodeBase64Char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static int decodeHexChar(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool decodeDataUri(tg3_str uri, std::vector<uint8_t>& out) {
    out.clear();
    if (!uri.data || uri.len < 6) return false;

    const std::string_view value(uri.data, uri.len);
    if (value.substr(0, 5) != "data:") return false;

    const size_t comma = value.find(',');
    if (comma == std::string_view::npos) return false;

    const std::string_view metadata = value.substr(5, comma - 5);
    const std::string_view payload = value.substr(comma + 1);
    const bool isBase64 = metadata.contains(";base64");

    if (!isBase64) {
        out.reserve(payload.size());
        for (size_t i = 0; i < payload.size(); ++i) {
            if (payload[i] == '%' && i + 2 < payload.size()) {
                const int hi = decodeHexChar(payload[i + 1]);
                const int lo = decodeHexChar(payload[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    out.push_back(static_cast<uint8_t>((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }
            out.push_back(static_cast<uint8_t>(payload[i]));
        }
        return !out.empty();
    }

    int val = 0;
    int valBits = -8;
    out.reserve((payload.size() * 3u) / 4u);
    for (char c : payload) {
        if (c == '=') break;
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') continue;
        const int decoded = decodeBase64Char(c);
        if (decoded < 0) return false;
        val = (val << 6) | decoded;
        valBits += 6;
        if (valBits >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valBits) & 0xff));
            valBits -= 8;
        }
    }
    return !out.empty();
}

static bool getImageBufferView(const tg3_model& model, const tg3_image& image, EncodedImageView& out) {
    out = {};
    if (image.buffer_view < 0 || image.buffer_view >= static_cast<int32_t>(model.buffer_views_count)) return false;

    const tg3_buffer_view& bufferView = model.buffer_views[image.buffer_view];
    if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int32_t>(model.buffers_count)) return false;

    const tg3_buffer& buffer = model.buffers[bufferView.buffer];
    if (!buffer.data.data || buffer.data.count == 0 || bufferView.byte_length == 0) return false;
    if (bufferView.byte_offset > buffer.data.count || bufferView.byte_length > buffer.data.count - bufferView.byte_offset) return false;

    out.data = buffer.data.data + bufferView.byte_offset;
    out.size = static_cast<size_t>(bufferView.byte_length);
    return true;
}

static bool decodeEncodedImageToRGBA(const uint8_t* data, size_t size, ConvertedTinyImage& converted, int& width, int& height) {
    converted.pixels.clear();
    converted.format = bgfx::TextureFormat::Unknown;
    width = 0;
    height = 0;
    if (!data || size == 0 || size > static_cast<size_t>(std::numeric_limits<int>::max())) return false;

    int channelsInFile = 0;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channelsInFile, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) stbi_image_free(pixels);
        return false;
    }

    const size_t pixelBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    converted.pixels.assign(pixels, pixels + pixelBytes);
    converted.format = bgfx::TextureFormat::RGBA8;
    stbi_image_free(pixels);
    return true;
}

static bool gltfTextureUsesMipmaps(const tg3_model& model, const tg3_texture& texture) {
    if (texture.sampler < 0 || texture.sampler >= static_cast<int32_t>(model.samplers_count)) return true;

    const int minFilter = model.samplers[texture.sampler].min_filter;
    return minFilter != TG3_TEXTURE_FILTER_NEAREST && minFilter != TG3_TEXTURE_FILTER_LINEAR;
}

static Mesh* loadPrimitiveWithDecodedBufferViews(const tg3_model& model, const tg3_primitive& prim, bool flipV, bool optimize, const std::unordered_map<int, MeshoptBufferView>& decodedBufferViews) {
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();

    // indices
    if (prim.indices >= static_cast<int32_t>(model.accessors_count)) {
        throw std::runtime_error("primitive indices accessor is out of range");
    }

    if (prim.indices >= 0) {
        const auto& acc = model.accessors[prim.indices];
        auto ia = accessOf(model, acc, decodedBufferViews);
        mesh->getIndexBuffer().m_indices.resize(ia.count);
        for (size_t i = 0; i < ia.count; ++i) {
            const unsigned char* p = ia.data + i * ia.stride;
            uint32_t idx = 0;
            switch (ia.componentType) {
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:  idx = *p; break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: idx = *reinterpret_cast<const uint16_t*>(p); break;
            case TG3_COMPONENT_TYPE_UNSIGNED_INT:   idx = *reinterpret_cast<const uint32_t*>(p); break;
            default: throw std::runtime_error("unsupported index type");
            }
            mesh->getIndexBuffer().m_indices[i] = idx;
        }
    }

    // attributes
    const tg3_accessor* pos = nullptr, * nrm = nullptr, * tan = nullptr, * uv0 = nullptr, * uv1 = nullptr, * col = nullptr, * jnt = nullptr, * wgt = nullptr;
    for (uint32_t attrIndex = 0; attrIndex < prim.attributes_count; ++attrIndex) {
        const tg3_str_int_pair& kv = prim.attributes[attrIndex];
        if (kv.value < 0 || kv.value >= static_cast<int32_t>(model.accessors_count)) {
            continue;
        }
        const auto& acc = model.accessors[kv.value];
        if (tg3StringEquals(kv.key, "POSITION")) pos = &acc;
        else if (tg3StringEquals(kv.key, "NORMAL")) nrm = &acc;
        else if (tg3StringEquals(kv.key, "TANGENT")) tan = &acc;
        else if (tg3StringEquals(kv.key, "TEXCOORD_0")) uv0 = &acc;
        else if (tg3StringEquals(kv.key, "TEXCOORD_1")) uv1 = &acc;
        else if (tg3StringEquals(kv.key, "COLOR_0")) col = &acc;
        else if (tg3StringEquals(kv.key, "JOINTS_0")) jnt = &acc;
        else if (tg3StringEquals(kv.key, "WEIGHTS_0")) wgt = &acc;
    }
    if (!pos) throw std::runtime_error("POSITION missing");

    auto ap = accessOf(model, *pos, decodedBufferViews);
    auto an = nrm ? accessOf(model, *nrm, decodedBufferViews) : AccessView{};
    auto at = tan ? accessOf(model, *tan, decodedBufferViews) : AccessView{};
    auto au0 = uv0 ? accessOf(model, *uv0, decodedBufferViews) : AccessView{};
    auto au1 = uv1 ? accessOf(model, *uv1, decodedBufferViews) : AccessView{};
    auto ac = col ? accessOf(model, *col, decodedBufferViews) : AccessView{};
    auto aj = jnt ? accessOf(model, *jnt, decodedBufferViews) : AccessView{};
    auto aw = wgt ? accessOf(model, *wgt, decodedBufferViews) : AccessView{};

    mesh->getVertexBuffer().m_vertices.resize(ap.count);

    for (size_t i = 0; i < ap.count; ++i) {
        float tmp[4];
        vertex v;
        readVecN<3>(ap, i, tmp); v.position = { tmp[0], tmp[1], tmp[2] };
        if (nrm) { readVecN<3>(an, i, tmp); v.normal = { tmp[0], tmp[1], tmp[2] }; } else v.normal = { 0,1,0 };
#ifndef ORKA_RENDERER_DISABLE_PBR
        if (tan) { readVecN<4>(at, i, tmp); v.tangent = { tmp[0], tmp[1], tmp[2] }; } else v.tangent = { 0,0,0 };
        if (uv1) { readVecN<2>(au1, i, tmp); v.uv1 = { tmp[0], flipV ? 1.0f - tmp[1] : tmp[1] }; } else v.uv1 = { 0,0 };
#endif
        if (uv0) { readVecN<2>(au0, i, tmp); v.uv0 = { tmp[0], flipV ? 1.0f - tmp[1] : tmp[1] }; } else v.uv0 = { 0,0 };
        if (col) {
            if (ac.type == TG3_TYPE_VEC4) {
                readVecN<4>(ac, i, tmp);
                v.color = Color{ u8FromNormFloat(tmp[0]), u8FromNormFloat(tmp[1]), u8FromNormFloat(tmp[2]), u8FromNormFloat(tmp[3]) };
            } else {
                readVecN<3>(ac, i, tmp);
                v.color = Color{ u8FromNormFloat(tmp[0]), u8FromNormFloat(tmp[1]), u8FromNormFloat(tmp[2]), 255 };
            }
        } else v.color = Color{ 1,1,1,1 };
#ifndef ORKA_RENDERER_DISABLE_SKINNING
        if (jnt && aw.type == TG3_TYPE_VEC4 && aj.type == TG3_TYPE_VEC4) {
            const unsigned char* pj = aj.data + i * aj.stride;
            uint16_t j[4] = { 0,0,0,0 };
            if (aj.componentType == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* q = reinterpret_cast<const uint8_t*>(pj);
                for (int k = 0; k < 4; ++k) j[k] = uint16_t(q[k]);
            } else if (aj.componentType == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* q = reinterpret_cast<const uint16_t*>(pj);
                for (int k = 0; k < 4; ++k) j[k] = q[k];
            } else if (aj.componentType == TG3_COMPONENT_TYPE_UNSIGNED_INT) {
                const uint32_t* q = reinterpret_cast<const uint32_t*>(pj);
                for (int k = 0; k < 4; ++k) j[k] = uint16_t(std::min<uint32_t>(q[k], 65535));
            } else {
                throw std::runtime_error("unsupported JOINTS_0 type");
            }
            v.joints = { int(j[0]), int(j[1]), int(j[2]), int(j[3]) };

            const unsigned char* pw = aw.data + i * aw.stride;
            uint16_t w[4] = { 0,0,0,0 };
            if (aw.componentType == TG3_COMPONENT_TYPE_FLOAT) {
                float f[4]; readVecN<4>(aw, i, f);
                float s = f[0] + f[1] + f[2] + f[3]; if (s > 0) for (float& x : f) x /= s;
                for (int k = 0; k < 4; ++k) w[k] = u16FromNormFloat(f[k]);
            } else if (aw.componentType == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* q = reinterpret_cast<const uint8_t*>(pw);
                uint32_t sum = uint32_t(q[0]) + q[1] + q[2] + q[3];
                uint8_t qn[4] = { q[0], q[1], q[2], q[3] };
                if (sum != 255 && sum != 0) {
                    float inv = 255.0f / float(sum);
                    for (int k = 0; k < 4; ++k) qn[k] = uint8_t(std::lround(q[k] * inv));
                }
                for (int k = 0; k < 4; ++k) w[k] = u16FromU8(qn[k]);
            } else if (aw.componentType == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* q = reinterpret_cast<const uint16_t*>(pw);
                uint32_t sum = uint32_t(q[0]) + q[1] + q[2] + q[3];
                uint16_t qn[4] = { q[0], q[1], q[2], q[3] };
                if (sum != 65535 && sum != 0) {
                    double inv = 65535.0 / double(sum);
                    for (int k = 0; k < 4; ++k) qn[k] = uint16_t(std::lround(q[k] * inv));
                }
                for (int k = 0; k < 4; ++k) w[k] = qn[k];
            } else {
                throw std::runtime_error("unsupported WEIGHTS_0 type");
            }
            v.weights = { int(w[0]), int(w[1]), int(w[2]), int(w[3]) };
        } else {
            v.joints = { 0,0,0,0 };
            v.weights = { 0,0,0,0 };
        }
#endif
        mesh->getVertexBuffer().m_vertices[i] = v;
    }

    if (mesh->getIndexBuffer().m_indices.empty()) {
        mesh->getIndexBuffer().m_indices.resize(mesh->getVertexBuffer().m_vertices.size());
        for (size_t i = 0; i < mesh->getIndexBuffer().m_indices.size(); ++i) mesh->getIndexBuffer().m_indices[i] = static_cast<uint32_t>(i);
    }

    #ifdef ORKA_ENABLE_MESHOPT
    if (optimize) {
        std::vector<uint32_t> remap(mesh->getIndexBuffer().m_indices.size());
        size_t unique = meshopt_generateVertexRemap(
            remap.data(),
            mesh->getIndexBuffer().m_indices.data(), mesh->getIndexBuffer().m_indices.size(),
            mesh->getVertexBuffer().m_vertices.data(), mesh->getVertexBuffer().m_vertices.size(), sizeof(mesh->getVertexBuffer().m_vertices[0]));

        std::vector<vertex> vtx(unique);
        std::vector<uint32_t> idx(mesh->getIndexBuffer().m_indices.size());

        meshopt_remapVertexBuffer(vtx.data(), mesh->getVertexBuffer().m_vertices.data(), mesh->getVertexBuffer().m_vertices.size(), sizeof(mesh->getVertexBuffer().m_vertices[0]), remap.data());
        meshopt_remapIndexBuffer(reinterpret_cast<uint32_t*>(idx.data()),
            reinterpret_cast<const uint32_t*>(mesh->getIndexBuffer().m_indices.data()),
            mesh->getIndexBuffer().m_indices.size(), remap.data());

        mesh->getVertexBuffer().m_vertices.swap(vtx);
        mesh->getIndexBuffer().m_indices.swap(idx);

        meshopt_optimizeVertexCache(reinterpret_cast<uint32_t*>(mesh->getIndexBuffer().m_indices.data()),
            reinterpret_cast<const uint32_t*>(mesh->getIndexBuffer().m_indices.data()),
            mesh->getIndexBuffer().m_indices.size(), mesh->getVertexBuffer().m_vertices.size());
        meshopt_optimizeOverdraw(reinterpret_cast<uint32_t*>(mesh->getIndexBuffer().m_indices.data()),
            reinterpret_cast<const uint32_t*>(mesh->getIndexBuffer().m_indices.data()),
            mesh->getIndexBuffer().m_indices.size(),
            reinterpret_cast<const float*>(mesh->getVertexBuffer().m_vertices.data()),
            mesh->getVertexBuffer().m_vertices.size(), sizeof(mesh->getVertexBuffer().m_vertices[0]), 1.05f);
        meshopt_optimizeVertexFetch(mesh->getVertexBuffer().m_vertices.data(),
            mesh->getIndexBuffer().m_indices.data(),
            mesh->getIndexBuffer().m_indices.size(),
            mesh->getVertexBuffer().m_vertices.data(), mesh->getVertexBuffer().m_vertices.size(),
            sizeof(mesh->getVertexBuffer().m_vertices[0]));
    }
    #endif

    return mesh.release();
}

Mesh* FileLoader::loadPrimitive(const tg3_model& model, const tg3_primitive& prim, bool flipV, bool optimize) {
    std::unordered_map<int, MeshoptBufferView> decodedBufferViews;
#ifdef ORKA_ENABLE_MESHOPT
    std::string meshoptError;
    if (!decodeMeshoptBufferViews(model, decodedBufferViews, meshoptError)) throw std::runtime_error(meshoptError);
#endif
    return loadPrimitiveWithDecodedBufferViews(model, prim, flipV, optimize, decodedBufferViews);
}

Model* FileLoader::loadGLTFModel(std::string_view filepath, bool optimize, bool flipV, bool logSceneNodeAssets) {
    ScopedTg3Model scopedModel;
    tg3_model& model = scopedModel.model;
    tg3_error_stack errors;
    tg3_error_stack_init(&errors);
    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.parse_float32 = 1;
    options.images_as_is = 1;
    options.preserve_image_channels = 1;
    const std::string filepathString(filepath);

    const tg3_error_code result = tg3_parse_file(
        &model,
        &errors,
        filepathString.c_str(),
        static_cast<uint32_t>(filepathString.size()),
        &options
    );

    if (result != TG3_OK || tg3_errors_has_error(&errors)) {
        const std::string err = tg3ErrorSummary(errors);
        tg3_error_stack_free(&errors);
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "Failed to parse glTF file '" + filepathString + "' apparently it identifies as modern art.\n\tError: " + err);
        return nullptr;
    }
    tg3_error_stack_free(&errors);

    if (tg3StringListContains(model.extensions_required, model.extensions_required_count, "KHR_draco_mesh_compression")) {
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "File '" + filepathString + "' requires KHR_draco_mesh_compression. Draco support has been removed; export the model without mesh compression.");
        return nullptr;
    }

    std::unordered_map<int, MeshoptBufferView> decodedMeshoptBufferViews;
#ifdef ORKA_ENABLE_MESHOPT
    std::string meshoptError;
    if (!decodeMeshoptBufferViews(model, decodedMeshoptBufferViews, meshoptError)) {
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "Failed to decode meshopt-compressed data in '" + filepathString + "': " + meshoptError);
        return nullptr;
    }
#else
    if (tg3StringListContains(model.extensions_required, model.extensions_required_count, "EXT_meshopt_compression") ||
        tg3StringListContains(model.extensions_required, model.extensions_required_count, "KHR_meshopt_compression")) {
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "File '" + filepathString + "' requires meshopt compression, but meshoptimizer is disabled.");
        return nullptr;
    }
#endif

    if (model.scenes_count == 0) {
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "File '" + filepathString + "' contains no scenes.\n\tEither minimalism went too far or the export just failed.");
        return nullptr;
    }

    int defaultScene = model.default_scene >= 0 ? model.default_scene : 0;
    if (defaultScene >= static_cast<int>(model.scenes_count)) defaultScene = 0;
    const tg3_scene& scene = model.scenes[defaultScene];

    if (scene.nodes_count > 1) {
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "File '" + filepathString + "' contains multiple root nodes (" + std::to_string(scene.nodes_count) + ").\n\tThat's not a model, that's a small ecosystem. Use a Scene instead!");
        return nullptr;
    }

    if (scene.nodes_count == 0) {
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "File '" + filepathString + "' contains no nodes.\n\tEither minimalism went too far or the export just failed.");
        return nullptr;
    }

    if (logSceneNodeAssets) logLargeSceneNodeGeometry(filepathString, model);

    auto orkaModel = new Model();
    std::unordered_map<int, std::shared_ptr<Texture>> texturePrototypes;
    const std::filesystem::path modelDir = std::filesystem::path(filepathString).parent_path();
    const std::string canonicalModelPath = canonicalAssetPath(filepathString);

    auto uploadGLTFTextureHandle = [](const ConvertedTinyImage& image, int width, int height, bool generateMips) -> bgfx::TextureHandle {
        if (image.pixels.empty() || image.format == bgfx::TextureFormat::Unknown || width <= 0 || height <= 0) return BGFX_INVALID_HANDLE;
        if (width > std::numeric_limits<uint16_t>::max() || height > std::numeric_limits<uint16_t>::max()) return BGFX_INVALID_HANDLE;

        std::vector<uint8_t> pixels;
        if (!buildMipChain(image.pixels.data(), width, height, generateMips, pixels)) return BGFX_INVALID_HANDLE;

        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
        Scheduler::runOnMainThreadBlocking([&handle, &pixels, width, height, generateMips, format = image.format]() {
            const bgfx::Memory* mem = bgfx::copy(pixels.data(), static_cast<uint32_t>(pixels.size()));
            handle = bgfx::createTexture2D(
                static_cast<uint16_t>(width),
                static_cast<uint16_t>(height),
                generateMips,
                1,
                format,
                resolveTextureFilteringFlags(),
                mem
            );
        });

        return handle;
    };

    auto loadTexturePrototype = [&](int textureIndex) -> std::shared_ptr<Texture> {
        if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures_count)) return {};
        const tg3_texture& srcTexture = model.textures[textureIndex];
        if (srcTexture.source < 0 || srcTexture.source >= static_cast<int>(model.images_count)) return {};

        const tg3_image& srcImage = model.images[srcTexture.source];
        const bool generateMips = gltfTextureUsesMipmaps(model, srcTexture);
        const std::string uri = tg3String(srcImage.uri);
        const bool externalFile = !uri.empty() && uri.rfind("data:", 0) != 0;
        const std::string resource = externalFile
            ? "file=" + canonicalAssetPath(modelDir / uri)
            : "model=" + canonicalModelPath + "|image=" + std::to_string(srcTexture.source);
        const std::string textureCacheKey = resource + "|mips=" + (generateMips ? "1" : "0") +
            "|flags=" + std::to_string(resolveTextureFilteringFlags());
        if (std::shared_ptr<Texture> cached = findCachedGLTFTexture(textureCacheKey)) return cached;

        if (externalFile) {
            Texture texture = FileLoader::loadImage((modelDir / uri).string(), generateMips, false, logSceneNodeAssets);
            if (texture) return cacheGLTFTexture(textureCacheKey, std::make_shared<Texture>(std::move(texture)));
        }

        ConvertedTinyImage convertedImage;
        bool textureLoadLogged = false;
        auto announceLargeTexture = [&](int width, int height) {
            if (!logSceneNodeAssets || textureLoadLogged) return;
            std::string label = tg3String(srcImage.uri);
            if (label.rfind("data:", 0) == 0) label.clear();
            if (label.empty()) label = tg3String(srcImage.name);
            if (label.empty()) label = "embedded texture " + std::to_string(srcTexture.source);
            logLargeSceneNodeTexture(label, width, height);
            textureLoadLogged = true;
        };
        auto uploadConvertedTexture = [&](const ConvertedTinyImage& image, int width, int height) -> std::shared_ptr<Texture> {
            announceLargeTexture(width, height);
            bgfx::TextureHandle handle = uploadGLTFTextureHandle(image, width, height, generateMips);
            if (!bgfx::isValid(handle)) return {};

            std::shared_ptr<Texture> out = std::make_shared<Texture>(handle);
            out->setWidth(static_cast<uint16_t>(width));
            out->setHeight(static_cast<uint16_t>(height));
            out->setFormat(image.format);
            out->setFlags(resolveTextureFilteringFlags());
            return out;
        };

        // Announce decoded images before format conversion and GPU upload begin.
        if (srcImage.width > 0 && srcImage.height > 0) announceLargeTexture(srcImage.width, srcImage.height);
        if (convertTinyImageToRGBA(srcImage, convertedImage)) {
            return cacheGLTFTexture(textureCacheKey, uploadConvertedTexture(convertedImage, srcImage.width, srcImage.height));
        }

        EncodedImageView encodedImage;
        if (getImageBufferView(model, srcImage, encodedImage)) {
            int width = 0;
            int height = 0;
            int channels = 0;
            if (stbi_info_from_memory(encodedImage.data, static_cast<int>(encodedImage.size), &width, &height, &channels)) {
                announceLargeTexture(width, height);
            }
            width = 0;
            height = 0;
            if (decodeEncodedImageToRGBA(encodedImage.data, encodedImage.size, convertedImage, width, height)) {
                return cacheGLTFTexture(textureCacheKey, uploadConvertedTexture(convertedImage, width, height));
            }
        }

        std::vector<uint8_t> dataUriBytes;
        if (decodeDataUri(srcImage.uri, dataUriBytes)) {
            int width = 0;
            int height = 0;
            int channels = 0;
            if (stbi_info_from_memory(dataUriBytes.data(), static_cast<int>(dataUriBytes.size()), &width, &height, &channels)) {
                announceLargeTexture(width, height);
            }
            width = 0;
            height = 0;
            if (decodeEncodedImageToRGBA(dataUriBytes.data(), dataUriBytes.size(), convertedImage, width, height)) {
                return cacheGLTFTexture(textureCacheKey, uploadConvertedTexture(convertedImage, width, height));
            }
        }

        if (srcImage.uri.len > 0) {
            const std::string uri = tg3String(srcImage.uri);
            if (uri.rfind("data:", 0) == 0) return {};
            Texture texture = FileLoader::loadImage((modelDir / uri).string(), generateMips, false, logSceneNodeAssets);
            if (texture) return cacheGLTFTexture(textureCacheKey, std::make_shared<Texture>(std::move(texture)));
        }

        return {};
    };

    auto loadDirectTextureInstance = [&](int textureIndex) -> std::shared_ptr<Texture> {
        if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures_count)) return {};
        const tg3_texture& srcTexture = model.textures[textureIndex];
        if (srcTexture.source < 0 || srcTexture.source >= static_cast<int>(model.images_count)) return {};
        const tg3_image& srcImage = model.images[srcTexture.source];
        const bool generateMips = gltfTextureUsesMipmaps(model, srcTexture);
        if (srcImage.uri.len == 0) return {};
        const std::string uri = tg3String(srcImage.uri);
        if (uri.rfind("data:", 0) == 0) return {};
        Texture texture = FileLoader::loadImage((modelDir / uri).string(), generateMips, false, logSceneNodeAssets);
        if (!texture) return {};
        return std::make_shared<Texture>(std::move(texture));
    };

    auto loadPrimitiveMaterial = [&](const tg3_primitive& primitive) -> Material* {
        if (primitive.material < 0 || primitive.material >= static_cast<int>(model.materials_count)) return nullptr;

        const tg3_material& srcMaterial = model.materials[primitive.material];
        Material* material = new Material();
        material->setShadingMode(MSM_UNLIT);

        std::vector<int> textureIndices;
        textureIndices.reserve(5);
        if (srcMaterial.pbr_metallic_roughness.base_color_texture.index >= 0) textureIndices.push_back(srcMaterial.pbr_metallic_roughness.base_color_texture.index);
        if (srcMaterial.pbr_metallic_roughness.metallic_roughness_texture.index >= 0) textureIndices.push_back(srcMaterial.pbr_metallic_roughness.metallic_roughness_texture.index);
        if (srcMaterial.normal_texture.index >= 0) textureIndices.push_back(srcMaterial.normal_texture.index);
        if (srcMaterial.occlusion_texture.index >= 0) textureIndices.push_back(srcMaterial.occlusion_texture.index);
        if (srcMaterial.emissive_texture.index >= 0) textureIndices.push_back(srcMaterial.emissive_texture.index);

        std::unordered_set<int> uniqueIndices;
        for (int texIndex : textureIndices) {
            if (!uniqueIndices.insert(texIndex).second) continue;

            std::shared_ptr<Texture> prototype;
            auto it = texturePrototypes.find(texIndex);
            if (it != texturePrototypes.end()) prototype = it->second;
            else {
                prototype = loadTexturePrototype(texIndex);
                texturePrototypes[texIndex] = prototype;
            }

            if (prototype != nullptr) {
                material->addTexture(prototype);
                continue;
            }

            std::shared_ptr<Texture> directTexture = loadDirectTextureInstance(texIndex);
            if (directTexture != nullptr) material->addTexture(directTexture);
        }

        return material;
    };

    for (uint32_t meshIndex = 0; meshIndex < model.meshes_count; ++meshIndex) {
        const tg3_mesh& m = model.meshes[meshIndex];
        for (uint32_t primitiveIndex = 0; primitiveIndex < m.primitives_count; ++primitiveIndex) {
            const tg3_primitive& p = m.primitives[primitiveIndex];
            Mesh* mesh = nullptr;
            try {
                mesh = loadPrimitiveWithDecodedBufferViews(model, p, flipV, optimize, decodedMeshoptBufferViews);
            }
            catch (const std::exception& e) {
                Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "Skipping malformed glTF primitive in '" + filepathString + "': " + e.what());
            }
            if (mesh) {
                orkaModel->addMesh(mesh);
                Material* material = loadPrimitiveMaterial(p);
                if (material != nullptr) orkaModel->addMaterial(mesh, material);
            }
        }
    }

    texturePrototypes.clear();

    if (orkaModel->getMeshes().empty()) {
        Logger::log(LET_ERROR, "Orka, GLTF Fileloader", "File '" + filepathString + "' contains no loadable mesh primitives.");
        delete orkaModel;
        return nullptr;
    }

    // Todo: add PBR Bones and animation.

    return orkaModel;
}
