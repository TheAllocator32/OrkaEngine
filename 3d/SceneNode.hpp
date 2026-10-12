/*
 * File: SceneNode.hpp
 *
 * Defines the SceneNode base structure
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <functional>
#include <mutex>
#include <utility>
#include <type_traits>
#include <typeinfo>
#include <vector>
#include "../primitives/core/MathTypes.hpp"
#include "../whaleml/WhaleMLFile.hpp"
#include "../essentials/Identifier.hpp"

enum SceneNodeType { // Self explaining...
    SNT_USELESS,
    SNT_MODEL,
    SNT_LIGHT,
    SNT_CAMERA,
    SNT_AUDIO,
    SNT_BILLBOARD,
    SNT_PARTICLE_EMITTER
};

class SceneManager; // Rage: I HATE FORWARD DECLs!!!
class SceneNode; // Oh damn. Compilers are strange translators...

class SceneNode {
public:
    // Creates and registers a SceneNode. Use this to create a SceneNode!
    static SceneNode& create();

    // Don't allow "SceneNode n = SceneNode::create"!
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;
    SceneNode(SceneNode&&) = delete;
    SceneNode& operator=(SceneNode&&) = delete;

    // Creates the SceneNode (e.g. for the model this is done by loadFile). If instanceScript is true, the script will be instanced (a new Lua state will be created for this SceneNode) so the global script variables won't collide with other scripted SceneNodes!
    virtual void create(std::string_view scriptPath, bool instanceScript=true);
    // Destroys the SceneNode removes the SceneNode from the SceneManager at the end of the frame
    virtual void destroy();
    // Clones the SceneNode (creates a new SceneNode with the same properties)
    inline SceneNode& clone() const { return cloneToType<SceneNode>(); }
    // Returns the script path attached to this SceneNode.
    inline std::string_view getScriptPath() const { return m_scriptPath; }
    // Returns the WhaleML name attached to this SceneNode.
    inline std::string_view getName() const { return m_name; }

    // Returns the ID of the SceneNode
    inline Identifier getID() const { return m_id; }
    constexpr operator Identifier() const { return m_id; }

    // Casts this SceneNode to T, throws std::bad_cast when it is not that type
    template<typename T>
    inline T& as() {
        static_assert(std::is_base_of_v<SceneNode, T>, "T must inherit SceneNode");
        return dynamic_cast<T&>(*this);
    }
    // Casts this SceneNode to T, throws std::bad_cast when it is not that type
    template<typename T>
    inline const T& as() const {
        static_assert(std::is_base_of_v<SceneNode, T>, "T must inherit SceneNode");
        return dynamic_cast<const T&>(*this);
    }

    // Sets the position of the SceneNode
    inline void setPosition(const vector3df& pos) { setPosition(pos, true); }
    // Sets the position and controls whether the transform changed callback is invoked
    inline void setPosition(const vector3df& pos, bool callTransformChangedCallback) { m_position = pos; notifyTransformChanged(callTransformChangedCallback); }
    // Sets the position by numeric components
    inline void setPosition(double x, double y, double z) { setPosition(vector3df(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))); }
    // Returns the position of the SceneNode
    inline vector3df getPosition() const { return m_position; }
    // Sets the scale of the SceneNode
    inline void setScale(const vector3df& scale) { setScale(scale, true); }
    // Sets the scale and controls whether the transform changed callback is invoked
    inline void setScale(const vector3df& scale, bool callTransformChangedCallback) { m_scale = scale; notifyTransformChanged(callTransformChangedCallback); }
    // Sets the scale by numeric components
    inline void setScale(double x, double y, double z) { setScale(vector3df(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))); }
    // Returns the scale of the SceneNode
    inline vector3df getScale() const { return m_scale; }
    // Sets the rotation of the SceneNode (as a quaternion)
    inline void setRotation(const quaternion& rot) { setRotation(rot, true); }
    // Sets the rotation and controls whether the transform changed callback is invoked
    inline void setRotation(const quaternion& rot, bool callTransformChangedCallback) { m_rotation = rot; notifyTransformChanged(callTransformChangedCallback); }
    // Sets the rotation by numeric components
    inline void setRotation(double w, double x, double y, double z) { setRotation(quaternion(static_cast<float>(w), static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))); }
    // Returns the rotation of the SceneNode (as a quaternion)
    inline quaternion getRotation() const { return m_rotation; }

    // Returns the center point of the SceneNode
    inline virtual vector3df getCenterPoint() const { return m_position; }

    // Sets the callback invoked when transform components change
    inline void setTransformChangedCallback(std::function<void()> callback) { m_transformChangedCallback = std::move(callback); }
    
    // Sets whether the SceneNode should be rendered
    inline void setVisible(bool visible) { m_visible = visible; }
    // Returns whether the SceneNode should be rendered
    inline bool isVisible() const { return m_visible && !m_destroyPending; }

    // Returns whether this SceneNode is queued for deferred destruction
    inline bool isDestroyPending() const { return m_destroyPending; }
    inline bool isValid() const { return !m_destroyPending && m_id != Identifier::INVALID; }

    // Sets a custom user string (e.g. for identification, grouping etc...)
    inline void setCustomUserData(std::string_view tag) { m_customUserDataString = tag; }
    // Sets a WhaleMLFile object as custom user data! (Should be an in RAM WhaleMLFile without file)
    void setCustomUserWhaleMLFile(const WhaleMLFile& tag);
    // Returns the custom user data (string)
    inline std::string_view getCustomUserData() const { return m_customUserDataString; }
    // Returns the custom user WhaleML file (Can be modified!)
    inline WhaleMLFile& getCustomUserWhaleMLFile() { return *m_customUserWhaleMLFile; }

    inline virtual SceneNodeType getType() const { return SNT_USELESS; };
protected:
    friend class SceneManager;
    // Standard init
    SceneNode(bool addToSceneManager = true);
    virtual ~SceneNode() { /* Todo: Add sanity checker in idiot proof mode */ }
    inline void setID(Identifier id) { m_id = id; }
    inline Identifier getScriptID() const { return m_scriptID; }
    template<typename T, typename... Args>
    static T& create(Args&&... args);
    template<typename T>
    T& cloneToType() const;
    inline void setName(std::string_view name) { m_name = name; }
    inline void notifyTransformChanged(bool callTransformChangedCallback) { if (callTransformChangedCallback && m_transformChangedCallback) m_transformChangedCallback(); }
    // ScriptEngine owns thread-local Lua states. SceneManager executes this queue once per frame.
    static void queueScriptOperation(std::function<void()> operation);
    static bool executeQueuedScriptOperations();
    static void drainQueuedScriptOperations();
    void updateScript(float delta);
protected:
    Identifier m_id;
    Identifier m_scriptID = Identifier::INVALID, m_progressID = Identifier::INVALID;
    bool m_addedToSceneManager = false;
    bool m_visible = true;
    bool m_destroyPending = false;
    std::string m_customUserDataString;
    std::unique_ptr<WhaleMLFile> m_customUserWhaleMLFile;
    vector3df m_position = vector3df(0.0f, 0.0f, 0.0f);
    vector3df m_scale = vector3df(1.0f, 1.0f, 1.0f);
    quaternion m_rotation = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    std::function<void()> m_transformChangedCallback;
    std::string m_name;
    std::string m_scriptPath;
    bool m_instanceScript = true;
private:
    // Updates the SceneNode (called every frame by SceneManager)
    virtual void update(float delta);
    inline static std::mutex sm_scriptOperationsMutex;
    inline static std::vector<std::function<void()>> sm_scriptOperations;
};

struct NoParentDummyFromHell {};

template<typename ParentClass>
using SceneNodeBaseSelector = std::conditional_t<std::is_base_of_v<SceneNode, ParentClass>, ParentClass, SceneNode>;
template<typename Derived, typename ParentClass = NoParentDummyFromHell>
class CreatableSceneNode : public SceneNodeBaseSelector<ParentClass> { // Now this is where CRTP is actually useful!
public:
    using Base = SceneNodeBaseSelector<ParentClass>;

    template<typename... Args>
    static Derived& create(Args&&... args) { return SceneNode::create<Derived>(std::forward<Args>(args)...); }

    inline Derived& clone() const { return this->template cloneToType<Derived>(); }

protected:
    using Base::Base; // Copy/Inherit constructor
}; 

// Template stuff (sorry ':/)
template<typename T, typename... Args>
T& SceneNode::create(Args&&... args) {
    static_assert(std::is_base_of_v<SceneNode, T>, "Damn, HOW!? You passed a non-SceneNode-type to SceneNode::create!");
    return *new T(std::forward<Args>(args)...); // YES, this is intentional, NO this is no memory leak (SceneNodes automatically hook themselves onto SceneManager). YES there is a reason why we do so.
}

template<typename T>
T& SceneNode::cloneToType() const {
    static_assert(std::is_base_of_v<SceneNode, T>, "Damn, HOW!? You passed a non-SceneNode-type to SceneNode::clone!");
    T& newNode = T::create();
    newNode.m_position = m_position;
    newNode.m_scale = m_scale;
    newNode.m_rotation = m_rotation;
    newNode.m_visible = m_visible;
    newNode.m_customUserDataString = m_customUserDataString;
    newNode.m_name = m_name;
    if (m_customUserWhaleMLFile) {
        newNode.m_customUserWhaleMLFile = std::make_unique<WhaleMLFile>(*m_customUserWhaleMLFile);
    }
    return newNode;
}

