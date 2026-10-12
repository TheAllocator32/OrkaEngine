/*
 * File: WhaleMLFile.hpp
 *
 * A small file-backed wrapper around the WhaleML parser.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "../primitives/core/MathTypes.hpp"
#include "WhaleMLScalarValue.hpp"
#include "WhaleMLParser.hpp"

/*
 * Supported query functions:
 *
 * getValue(path)
 *   Format: key1.key2.key3... (arbitrary depth)
 *   Example: "player.lucas.settings.level"
 *   Returns one scalar value from a WhaleML object tree.
 *
 * getValues(path)
 *   Format: key1.key2.key3 if key3 is a list
 *   Example: "server.channels" returns all scalar channels in server.channels.
 *   WhaleML also supports nested lists through numbered sublist path parts:
 *   "key1.list.1" or "key1.list.1.1".
 *   Sublists count up from 1, so the first sublist is "1", not "0".
 *
 * Summary:
 *   - getValue(): single scalar value
 *   - getValues(): scalar values from a WhaleML list
 */

class WhaleMLFile {
public:
    WhaleMLFile() { clear(); }
    // Loads a WhaleML file immediately.
    explicit WhaleMLFile(std::string_view filepath) { load(filepath); }
    WhaleMLFile(const WhaleMLFile& other);
    WhaleMLFile& operator=(const WhaleMLFile& other);
    inline bool isValid() const { return m_valid; }

    // Loads a WhaleML file from disk.
    void load(std::string_view filepath);
    // Loads WhaleML directly from a string buffer, for example network data or generated config.
    void loadBuffer(std::string_view buffer);

    // Navigates to a path prefix. After navigate("account.friends"), getValue("name") reads "account.friends.name". Call without arguments or with "_ROOT_" to reset.
    void navigate(std::string_view point = "_ROOT_");
    inline std::string_view getCurrentNavPoint() const { return m_navPoint; }

    // Returns one scalar value. See the query syntax at the beginning of this file.
    WhaleMLScalarValue getValue(std::string_view key) const;
    // Returns whether a scalar value exists at the given path.
    inline bool hasKey(std::string_view key) const { return getValue(key).isValid(); }
    // Returns multiple scalar values from a WhaleML list. See the query syntax above.
    std::vector<WhaleMLScalarValue> getValues(std::string_view key) const;
    // Returns the names of direct children at a path. This is one layer only, not recursive.
    std::vector<std::string> getChildren(std::string_view key) const;

    // Operator [] uses getValue internally.
    inline WhaleMLScalarValue operator[](std::string_view key) const { return getValue(key); }
    // Operator () uses getValues internally.
    inline std::vector<WhaleMLScalarValue> operator()(std::string_view key) const { return getValues(key); }

    // Typed reads with defaults. VERY useful!
    std::string string(std::string_view key, std::string_view defaultValue = "") const;
    std::string string(std::string_view key, std::string_view defaultValue, bool& success) const;
    bool boolean(std::string_view key, bool defaultValue = false) const;
    bool boolean(std::string_view key, bool defaultValue, bool& success) const;
    int integer(std::string_view key, int defaultValue = 0) const;
    int integer(std::string_view key, int defaultValue, bool& success) const;
    float floating(std::string_view key, float defaultValue = 0.0f) const;
    float floating(std::string_view key, float defaultValue, bool& success) const;
    vector3df vector3d(std::string_view key, const vector3df& defaultValue = vector3df(0.0f)) const;
    vector3df vector3d(std::string_view key, const vector3df& defaultValue, bool& success) const;
    quaternionf quaternion(std::string_view key, const quaternionf& defaultValue = quaternionf(1.0f, 0.0f, 0.0f, 0.0f)) const;
    quaternionf quaternion(std::string_view key, const quaternionf& defaultValue, bool& success) const;

    // Writes a WhaleML key. If saveFile is true and this object was loaded from a file, the changed WhaleML is written back to disk.
    bool setValue(std::string_view key, std::string_view value, bool saveFile = false);
    inline bool setValue(std::string_view key, double value) { return setValue(key, std::to_string(value)); }
    inline bool setValue(std::string_view key, int value) { return setValue(key, std::to_string(value)); }
    inline bool setValue(std::string_view key, bool value) { return setValue(key, value ? "true" : "false"); }
    inline bool setValue(std::string_view key, const char* value) { return setValue(key, value, false); }
    // Writes a list of scalar values at a key.
    bool setValues(std::string_view key, const std::vector<std::string>& values, bool saveFile = false);

    // Dumps the loaded WhaleML tree to a string.
    std::string dump() const;
    std::string dump(WhaleMLPrettyPrintOptions options) const;
    template <typename... Options>
    std::string dump(Options... options) const { return dump(makeWhaleMLPrettyPrintOptions(options...)); }
    std::string dumpSingleLine() const;
    // Dumps the loaded WhaleML tree to a file.
    void dumpFile(std::string_view path) const;
    void dumpFile(std::string_view path, WhaleMLPrettyPrintOptions options) const;
    template <typename... Options>
    inline void dumpFile(std::string_view path, Options... options) const { dumpFile(path, makeWhaleMLPrettyPrintOptions(options...)); }

    // Clears all loaded data and resets navigation.
    void clear();

private:
    std::string resolvePath(std::string_view key) const;

    bool m_isFile = false;
    bool m_valid = false;
    std::unique_ptr<WhaleMLNode> m_whaleMLRoot;
    std::string m_navPoint = "_ROOT_";
    std::string m_filePath;
};

