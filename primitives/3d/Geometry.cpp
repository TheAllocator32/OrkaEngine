/*
 * File: Geometry.cpp
 *
 * Geometry primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Geometry.hpp"

AABB vertex_buffer::calculateAABB() const {
    if (m_vertices.empty()) return AABB();
    AABB box;
    box.setStartEnd(m_vertices[0].position, m_vertices[0].position);
    for (const auto& v : m_vertices) {
        box.expandToInclude(v.position);
    }
    return box;
}

BS vertex_buffer::calculateBS() const {
    if (m_vertices.empty()) return BS();
    BS sphere;
    sphere.set(m_vertices[0].position, 1.0f);
    for (const auto& v : m_vertices) {
        sphere.expandToInclude(v.position);
    }
    return sphere;
}

vertex_buffer vertex_buffer::clone() const {
    vertex_buffer vb;
    vb.m_vertices = m_vertices;
    vb.m_dirty = true;
    return vb;
}

bool index_buffer::is16Bit() const {
    for (uint32_t index : m_indices) {
        if (index > 65535) return false;
    }
    return true;
}

index_buffer index_buffer::clone() const {
    index_buffer ib;
    ib.m_indices = m_indices;
    ib.m_dirty = true;
    return ib;
}

Mesh::~Mesh() {
    clear();
}

Mesh* Mesh::clone() const {
    Mesh* m = new Mesh();
    m->m_vertices = m_vertices;
    m->m_indices = m_indices;
    m->m_cachedAABB = m_cachedAABB;
    m->m_cachedBS = m_cachedBS;
    m->m_boundsCacheValid = m_boundsCacheValid;
    return m;
}

void Mesh::setVertexBuffer(const vertex_buffer& vb) {
    m_vertices = vb;
    m_vertices.m_dirty = true;
    invalidateBoundsCache();
}

AABB Mesh::getAABB() const {
    updateBoundsCache();
    return m_cachedAABB;
}

BS Mesh::getBS() const {
    updateBoundsCache();
    return m_cachedBS;
}

void Mesh::clear() {
    m_vertices.m_vertices.clear();
    m_vertices.m_dirty = false;
    m_indices.m_indices.clear();
    m_indices.m_dirty = false;
    m_cachedAABB = AABB();
    m_cachedBS = BS();
    m_boundsCacheValid = false;
    if (bgfx::isValid(m_vertexBufferHandle)) {
        bgfx::destroy(m_vertexBufferHandle);
        m_vertexBufferHandle = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_indexBufferHandle)) {
        bgfx::destroy(m_indexBufferHandle);
        m_indexBufferHandle = BGFX_INVALID_HANDLE;
    }
}

Mesh* Mesh::clone() {
    Mesh* newMesh = new Mesh();
    newMesh->setVertexBuffer(m_vertices);
    newMesh->setIndexBuffer(m_indices);
    return newMesh;
}

void Mesh::updateBoundsCache() const {
    if (m_boundsCacheValid) return;
    m_cachedAABB = m_vertices.calculateAABB();
    m_cachedBS = m_vertices.calculateBS();
    m_boundsCacheValid = true;
}

