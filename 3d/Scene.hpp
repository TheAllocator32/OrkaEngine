/*
 * File: Scene.hpp
 *
 * Scene - sorts/groups SceneNodes
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <string>
#include <vector>
#include <span>
#include <memory>
#include <string_view>
#include "../primitives/core/MathTypes.hpp"
#include "../essentials/Identifier.hpp"

class WhaleMLFile;
class SceneManager;

class Scene {
public:
    class Chunk;
    struct LODStages {
        std::vector<int> stages;
        int numStages=0;
        int stepPatternToContinue=0;
    };

    static Scene& create();
    // Returns the ID assigned by SceneManager.
    inline Identifier getID() const { return m_id; }
    constexpr operator Identifier() const { return m_id; }

    // Loads the .scene.whaleml file
    void loadFile(std::string_view filepath);
    // Unloads scene and destroys all associated SceneNodes
    void destroy();

    // Moves the entire Scene (including chunks and SceneNodes) (Apply after loading the file!)
    void setPosition(vector3df pos);
    inline vector3df getPosition() const { return m_position; }

    // Returns all Orka SceneNodes that belong to this Scene (INCLUDING SCENENODES THAT BELONG TO CHUNKS!)
    inline std::vector<Identifier> getSceneNodes() const;
    inline std::span<const std::unique_ptr<Chunk>> getChunks() const { return m_chunks; }
    // Returns name set in .scene.whaleml
    inline std::string_view getName() const { return m_name; }

    // Returns the LOD stages defined in the .scene.whaleml
    inline LODStages getLODStages() const { return m_lodStages; }
protected:
    friend class SceneManager;
    Scene();
    ~Scene();
    void setID(Identifier id) { m_id = id; }
    Identifier m_id;
    std::vector<Identifier> m_sceneNodes;
    std::vector<std::unique_ptr<Chunk>> m_chunks;
    std::string m_name;
    bool m_hasChunks=false;
    bool m_loaded=false;
    LODStages m_lodStages;
    vector3df m_position;
};

class Scene::Chunk { // Looks weird but works(tm)
public:
    // Loads the .scene.whaleml file
    void loadFile(std::string_view filepath);
    // Unloads scene and destroys all associated SceneNodes
    void destroy();

    // Moves the entire Chunk (including SceneNodes) (Please apply after loading file!)
    void setPosition(vector3df pos);
    inline vector3df getPosition() const { return m_position; }

    // Returns all Orka SceneNodes that belong to this Chunk
    inline std::span<const Identifier> getSceneNodes() const { return m_sceneNodes; }
    // Returns name set in .chunk.whaleml
    inline std::string_view getName() const { return m_name; }

protected:
    inline Chunk() = default;
    inline ~Chunk() { destroy(); }
    friend struct std::default_delete<Chunk>; // This is evil
    friend class Scene;
    std::vector<Identifier> m_sceneNodes;
    std::string m_name;
    vector3df m_position;
};

