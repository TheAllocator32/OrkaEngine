/*
 * File: Geometry.hpp
 *
 * Vertex, Mesh...
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../core/MathTypes.hpp"
#include "../core/Color.hpp"
#include "../physics/BoundingShapes.hpp"
#include <bgfx/bgfx.h>
#include <string>
#include <utility>
#include <vector>

// Vertex Skinned (PBR compatible)
struct vertex {
    vector3df position; // Useless without
    vector3df normal;    // Could be calculated...
    vector2df uv0;        // Texturing
    Color color;        // Duh...
#ifndef ORKA_RENDERER_DISABLE_PBR
    vector3df tangent;    // Needed for Lighting
    vector2df uv1;        // WHAT THE HECK!?
#endif
#ifndef ORKA_RENDERER_DISABLE_SKINNING
    vector4di joints;   // uint8_t
    vector4di weights;  // uint8_t
#endif

    inline vertex& operator=(vector3df newPosition) { position=newPosition; return *this; }
    inline vertex& operator=(const vertex& other) {
        if (this == &other) return *this;
        position = other.position;
        normal = other.normal;
        uv0 = other.uv0;
        color = other.color;
#ifndef ORKA_RENDERER_DISABLE_PBR
        tangent = other.tangent;
        uv1 = other.uv1;
#endif
#ifndef ORKA_RENDERER_DISABLE_SKINNING
        joints = other.joints;
        weights = other.weights;
#endif
        return *this;
    }
};

struct vertex_buffer {
    std::vector<vertex> m_vertices; 
    bool m_dirty = false;
    // Constructs empty buffer
    inline vertex_buffer() = default;
    // Constructs from vector
    inline explicit vertex_buffer(std::vector<vertex> v) : m_vertices(std::move(v)), m_dirty(true) {}
    // Returns size
    inline size_t size() const { return m_vertices.size(); }
    // Returns true if empty
    inline bool empty() const { return m_vertices.empty(); }
    // Marks buffer dirty
    inline void markDirty() { m_dirty = true; }
    // Clears dirty flag
    inline void clearDirty() { m_dirty = false; }
    // Appends vertex
    inline void pushBack(const vertex& v) { m_vertices.push_back(v); m_dirty = true; }
    // Returns true if dirty
    inline bool isDirty() const { return m_dirty; }
    // Access operator []
    inline vertex& operator[](size_t index) { m_dirty = true; return m_vertices[index]; }
    inline const vertex& operator[](size_t index) const { return m_vertices[index]; }

    // Calculates the axis-aligned bounding box of the vertex buffer
    AABB calculateAABB() const;

    // Calculates the bounding sphere of the vertex buffer
    BS calculateBS() const;
    // Clears everything!
    inline void clear() { m_vertices.clear(); m_dirty = false; }
    // Clones the vertex buffer
    vertex_buffer clone() const;
};

struct index_buffer {
    std::vector<uint32_t> m_indices;
    bool m_dirty = false;
    // Constructs empty buffer
    index_buffer() = default;
    // Constructs from vector
    explicit index_buffer(std::vector<uint32_t> i) : m_indices(std::move(i)) { m_dirty = true; }
    // Returns size
    size_t size() const { return m_indices.size(); }
    // Returns true if empty
    bool empty() const { return m_indices.empty(); }
    // Appends index
    void pushBack(uint32_t i) { m_indices.push_back(i); m_dirty = true; }
    // Manually marks buffer dirty
    inline void markDirty() { m_dirty = true; }
    // Clears dirty flag
    inline void clearDirty() { m_dirty = false; }
    // Returns true if dirty
    inline bool isDirty() const { return m_dirty; }
    // Is 16-bit compatible
    bool is16Bit() const;
    // Access operator []
    inline uint32_t& operator[](size_t index) { m_dirty = true; return m_indices[index]; }
    inline const uint32_t& operator[](size_t index) const { return m_indices[index]; }
    // Clears everything!
    inline void clear() { m_indices.clear(); m_dirty = false; }
    // Clones the index buffer
    index_buffer clone() const;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh* clone() const;

    inline bgfx::VertexBufferHandle getVertexBufferBGFXHandle() const { return m_vertexBufferHandle; }
    inline bgfx::IndexBufferHandle getIndexBufferBGFXHandle() const { return m_indexBufferHandle; }
    inline void setVertexBufferBGFXHandle(bgfx::VertexBufferHandle handle) { m_vertexBufferHandle = handle; }
    inline void setIndexBufferBGFXHandle(bgfx::IndexBufferHandle handle) { m_indexBufferHandle = handle; }

    inline vertex_buffer& getVertexBuffer() { invalidateBoundsCache(); return m_vertices; }
    inline const vertex_buffer& getVertexBuffer() const { return m_vertices; }
    inline index_buffer& getIndexBuffer() { return m_indices; }
    inline const index_buffer& getIndexBuffer() const { return m_indices; }

    void setVertexBuffer(const vertex_buffer& vb);
    inline void setIndexBuffer(const index_buffer& ib) { m_indices = ib; m_indices.m_dirty = true; }
    inline bool isDirty() const { return m_vertices.isDirty() || m_indices.isDirty(); }

    AABB getAABB() const;

    BS getBS() const;

    void clear();

    Mesh* clone();
private:
    inline void invalidateBoundsCache() { m_boundsCacheValid = false; }

    void updateBoundsCache() const;

    friend class Renderer;
    vertex_buffer m_vertices;
    index_buffer m_indices;
    mutable AABB m_cachedAABB;
    mutable BS m_cachedBS;
    mutable bool m_boundsCacheValid = false;
    bgfx::VertexBufferHandle m_vertexBufferHandle = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_indexBufferHandle = BGFX_INVALID_HANDLE;
};

