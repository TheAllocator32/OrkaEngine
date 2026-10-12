/*
 * File: WhaleMLFile.cpp
 *
 * Implements the WhaleMLFile wrapper.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "WhaleMLFile.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include "../essentials/Logger.hpp"

WhaleMLFile::WhaleMLFile(const WhaleMLFile& other) {
    m_isFile = other.m_isFile;
    m_valid = other.m_valid;
    m_navPoint = other.m_navPoint;
    m_filePath = other.m_filePath;

    if (other.m_whaleMLRoot) {
        m_whaleMLRoot = other.m_whaleMLRoot->clone();
    }
}

WhaleMLFile& WhaleMLFile::operator=(const WhaleMLFile& other) {
    if (this == &other)
        return *this;

    m_isFile = other.m_isFile;
    m_valid = other.m_valid;
    m_navPoint = other.m_navPoint;
    m_filePath = other.m_filePath;
    m_whaleMLRoot = other.m_whaleMLRoot ? other.m_whaleMLRoot->clone() : std::make_unique<WhaleMLList>();

    return *this;
}

void WhaleMLFile::load(std::string_view filepath) {
    const std::string filepathString(filepath);
    std::ifstream file(filepathString);
    m_isFile = true;
    m_filePath = filepathString;
    m_valid = true;

    if (!file) {
        Logger::log(LET_WARN, "Orka, WhaleMLFile", filepath.empty() ? "Could not load WhaleML file because you passed an empty path. If this was intentional, carry on." : "Could not load WhaleML file: '" + filepathString + "'. If this was intentional, carry on.");
        m_valid = false;
        clear();
        m_valid = false;
        return;
    }

    try {
        m_whaleMLRoot = WhaleMLParser::parseFile(filepath);
        m_valid = true;
    }
    catch (const std::runtime_error& e) {
        Logger::log(LET_ERROR, "Orka, WhaleMLFile", "WhaleML parsing failed: " + std::string(e.what()));
        clear();
        m_valid = false;
    }
}

void WhaleMLFile::loadBuffer(std::string_view buffer) {
    try {
        m_whaleMLRoot = WhaleMLParser::parse(buffer);
        m_valid = true;
    }
    catch (const std::runtime_error& e) {
        Logger::log(LET_ERROR, "Orka, WhaleMLFile", "WhaleML parsing failed: " + std::string(e.what()));
        clear();
        m_valid = false;
    }
}

void WhaleMLFile::navigate(std::string_view point) {
    if (point.empty()) m_navPoint = "_ROOT_";
    else m_navPoint = point;
}

WhaleMLScalarValue WhaleMLFile::getValue(std::string_view key) const {
    if (!m_whaleMLRoot)
        return WhaleMLScalarValue("", false);

    WhaleMLNode* node = m_whaleMLRoot.get();
    std::stringstream ss{resolvePath(key)};
    std::string segment;

    while (std::getline(ss, segment, '.')) {
        auto& children = node->getChildren();
        auto it = children.find(segment);
        if (it == children.end()) {
            return WhaleMLScalarValue("", false);
        }
        node = it->second.get();
    }

    if (node->isList())
        return WhaleMLScalarValue("", false);

    return static_cast<WhaleMLValue*>(node)->getValue();
}

std::vector<WhaleMLScalarValue> WhaleMLFile::getValues(std::string_view key) const {
    std::vector<WhaleMLScalarValue> values;
    if (!m_whaleMLRoot)
        return values;

    auto isNumericSegment = [](std::string_view s) -> bool {
        return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    };

    WhaleMLNode* node = m_whaleMLRoot.get();
    std::stringstream ss{resolvePath(key)};
    std::string segment;

    while (std::getline(ss, segment, '.')) {
        if (!node->isList()) {
            auto it = node->getChildren().find(segment);
            if (it == node->getChildren().end())
                return values;
            node = it->second.get();
            continue;
        }

        WhaleMLList* list = static_cast<WhaleMLList*>(node);
        if (isNumericSegment(segment)) {
            int idx = std::stoi(segment);
            auto it2 = list->getSublists().end();
            if (idx > 0) {
                it2 = list->getSublists().find(idx - 1);
            }
            if (it2 == list->getSublists().end()) {
                it2 = list->getSublists().find(idx);
            }
            if (it2 != list->getSublists().end()) {
                node = &it2->second;
                continue;
            }
        }

        auto it = node->getChildren().find(segment);
        if (it == node->getChildren().end())
            return values;
        node = it->second.get();
    }

    if (!node->isList()) {
        WhaleMLValue* valueNode = static_cast<WhaleMLValue*>(node);
        if (valueNode->getValue().isValid()) {
            values.emplace_back(valueNode->getValue().asString(), true);
        }
        return values;
    }

    WhaleMLList* list = static_cast<WhaleMLList*>(node);
    for (const auto& ptr : list->getItems()) {
        if (ptr)
            values.push_back(*ptr);
        else
            values.emplace_back("", true);
    }

    return values;
}

std::vector<std::string> WhaleMLFile::getChildren(std::string_view key) const {
    std::vector<std::string> children;
    if (!m_whaleMLRoot)
        return children;

    WhaleMLNode* node = m_whaleMLRoot.get();
    const std::string path = resolvePath(key);
    if (!path.empty()) {
        std::stringstream ss{path};
        std::string segment;

        while (std::getline(ss, segment, '.')) {
            auto it = node->getChildren().find(segment);
            if (it == node->getChildren().end()) {
                return children;
            }
            node = it->second.get();
        }
    }

    node->forEachChildOrdered([&](const std::string& name, const WhaleMLNode*) {
        children.push_back(name);
    });
    return children;
}

std::string WhaleMLFile::string(std::string_view key, std::string_view defaultValue) const {
    bool success = false;
    return string(key, defaultValue, success);
}

std::string WhaleMLFile::string(std::string_view key, std::string_view defaultValue, bool& success) const {
    success = false;
    try {
        WhaleMLScalarValue value = getValue(key);
        if (!value.isValid())
            return std::string(defaultValue);
        success = true;
        return value.asString();
    } catch (...) {
        return std::string(defaultValue);
    }
}

bool WhaleMLFile::boolean(std::string_view key, bool defaultValue) const {
    bool success = false;
    return boolean(key, defaultValue, success);
}

bool WhaleMLFile::boolean(std::string_view key, bool defaultValue, bool& success) const {
    success = false;
    try {
        WhaleMLScalarValue value = getValue(key);
        if (!value.isValid())
            return defaultValue;
        const bool result = value.asBool();
        success = true;
        return result;
    } catch (...) {
        return defaultValue;
    }
}

int WhaleMLFile::integer(std::string_view key, int defaultValue) const {
    bool success = false;
    return integer(key, defaultValue, success);
}

int WhaleMLFile::integer(std::string_view key, int defaultValue, bool& success) const {
    success = false;
    try {
        WhaleMLScalarValue value = getValue(key);
        if (!value.isValid())
            return defaultValue;
        const int result = value.asInt();
        success = true;
        return result;
    } catch (...) {
        return defaultValue;
    }
}

float WhaleMLFile::floating(std::string_view key, float defaultValue) const {
    bool success = false;
    return floating(key, defaultValue, success);
}

float WhaleMLFile::floating(std::string_view key, float defaultValue, bool& success) const {
    success = false;
    try {
        WhaleMLScalarValue value = getValue(key);
        if (!value.isValid())
            return defaultValue;
        const float result = value.asFloat();
        success = true;
        return result;
    } catch (...) {
        return defaultValue;
    }
}

vector3df WhaleMLFile::vector3d(std::string_view key, const vector3df& defaultValue) const {
    bool success = false;
    return vector3d(key, defaultValue, success);
}

vector3df WhaleMLFile::vector3d(std::string_view key, const vector3df& defaultValue, bool& success) const {
    success = false;
    try {
        auto values = getValues(key);
        if (values.size() != 3)
            return defaultValue;
        vector3df result(values[0].asFloat(), values[1].asFloat(), values[2].asFloat());
        success = true;
        return result;
    } catch (...) {
        return defaultValue;
    }
}

quaternionf WhaleMLFile::quaternion(std::string_view key, const quaternionf& defaultValue) const {
    bool success = false;
    return quaternion(key, defaultValue, success);
}

quaternionf WhaleMLFile::quaternion(std::string_view key, const quaternionf& defaultValue, bool& success) const {
    success = false;
    try {
        auto values = getValues(key);
        if (values.size() == 4) {
            quaternionf result(values[0].asFloat(), values[1].asFloat(), values[2].asFloat(), values[3].asFloat());
            success = true;
            return result;
        }
        if (values.size() == 3) {
            quaternionf result(vector3df(values[0].asFloat(), values[1].asFloat(), values[2].asFloat()));
            success = true;
            return result;
        }
        return defaultValue;
    } catch (...) {
        return defaultValue;
    }
}

bool WhaleMLFile::setValue(std::string_view key, std::string_view value, bool saveFile) {
    if (!m_whaleMLRoot)
        m_whaleMLRoot = std::make_unique<WhaleMLList>();

    WhaleMLNode* node = m_whaleMLRoot.get();

    std::vector<std::string> parts;
    std::stringstream ss{resolvePath(key)};
    std::string seg;

    while (std::getline(ss, seg, '.'))
        parts.push_back(seg);

    if (parts.empty()) {
        Logger::log(LET_WARN, "Orka, WhaleMLFile", "setValue called with empty path, ignoring write.");
        return false;
    }

    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const std::string& part = parts[i];
        auto* child = node->getChild(part);
        if (!child) {
            auto listNode = std::make_unique<WhaleMLList>();
            child = listNode.get();
            node->setChild(part, std::move(listNode));
        }
        node = child;
    }

    const std::string& finalKey = parts.back();
    WhaleMLNode* existing = node->getChild(finalKey);
    if (existing && !existing->isList()) {
        static_cast<WhaleMLValue*>(existing)->setValue(value);
    } else {
        auto target = std::make_unique<WhaleMLValue>();
        target->setValue(value);
        if (existing) {
            target->copyChildrenFrom(*existing);
        }
        node->setChild(finalKey, std::move(target));
    }

    if (saveFile && m_isFile) {
        dumpFile(m_filePath, m_whaleMLRoot->getAutodetectedPrettyPrintOptions());
    }
    return true;
}

bool WhaleMLFile::setValues(std::string_view key, const std::vector<std::string>& values, bool saveFile) {
    if (!m_whaleMLRoot)
        m_whaleMLRoot = std::make_unique<WhaleMLList>();

    WhaleMLNode* node = m_whaleMLRoot.get();
    std::vector<std::string> parts;
    std::stringstream ss{resolvePath(key)};
    std::string segment;
    while (std::getline(ss, segment, '.'))
        parts.push_back(segment);

    if (parts.empty()) {
        Logger::log(LET_WARN, "Orka, WhaleMLFile", "setValues called with empty path, ignoring write.");
        return false;
    }

    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const std::string& part = parts[i];
        auto* child = node->getChild(part);
        if (!child) {
            auto listNode = std::make_unique<WhaleMLList>();
            child = listNode.get();
            node->setChild(part, std::move(listNode));
        }
        node = child;
    }

    auto target = std::make_unique<WhaleMLList>();
    target->setExplicitList();
    for (const std::string& value : values)
        target->getItems().push_back(std::make_unique<WhaleMLScalarValue>(value, true));

    const std::string& finalKey = parts.back();
    if (WhaleMLNode* existing = node->getChild(finalKey))
        target->copyChildrenFrom(*existing);
    node->setChild(finalKey, std::move(target));

    if (saveFile && m_isFile)
        dumpFile(m_filePath, m_whaleMLRoot->getAutodetectedPrettyPrintOptions());
    return true;
}

std::string WhaleMLFile::dump() const {
    return WhaleMLParser::serialize(m_whaleMLRoot);
}

std::string WhaleMLFile::dump(WhaleMLPrettyPrintOptions options) const {
    return WhaleMLParser::serialize(m_whaleMLRoot, options);
}

std::string WhaleMLFile::dumpSingleLine() const {
    return WhaleMLParser::serialize(m_whaleMLRoot, WPO_SINGLELINE);
}

void WhaleMLFile::dumpFile(std::string_view path) const {
    dumpFile(path, m_whaleMLRoot ? m_whaleMLRoot->getAutodetectedPrettyPrintOptions() : 0);
}

void WhaleMLFile::dumpFile(std::string_view path, WhaleMLPrettyPrintOptions options) const {
    std::ofstream file{std::string(path)};
    if (file.is_open()) {
        file << WhaleMLParser::serialize(m_whaleMLRoot, options);
    }
}

void WhaleMLFile::clear() {
    m_whaleMLRoot = std::make_unique<WhaleMLList>();
    m_navPoint = "_ROOT_";
    m_valid = true;
}

std::string WhaleMLFile::resolvePath(std::string_view key) const {
    if (m_navPoint == "_ROOT_") {
        return std::string(key);
    }
    if (key.empty()) {
        return m_navPoint;
    }
    return m_navPoint + "." + std::string(key);
}

