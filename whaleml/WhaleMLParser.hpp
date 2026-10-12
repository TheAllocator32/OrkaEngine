/*
 * File: WhaleMLParser.hpp
 *
 * Orka WhaleML parser. Just works(tm) do not touch!
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
#include <cstddef>
#include <memory>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include "WhaleMLScalarValue.hpp"

enum WhaleMLPrettyPrintOption {
    WPO_SINGLELINE              = 1 << 0, // WhaleML single-line mode
    WPO_ANTI_REDUNDANT          = 1 << 1, // Enables anti-redundant lists (does not work with WPO_SINGLELINE)
    WPO_USE_EQUALS_SEPARATOR    = 1 << 2, // Uses '=' character as separator
    WPO_TIDY                    = 1 << 3, // Separates logical groups with empty lines
    WPO_NO_WHITESPACES          = 1 << 4, // Does not insert whitespace around ':', '/' and '='
    WPO_TRIM_QUOTES             = 1 << 5  // Removes quotes if value contains no whitespaces
};

using WhaleMLPrettyPrintOptions = unsigned int;

constexpr WhaleMLPrettyPrintOptions operator|(WhaleMLPrettyPrintOption lhs, WhaleMLPrettyPrintOption rhs) {
    return static_cast<WhaleMLPrettyPrintOptions>(lhs) | static_cast<WhaleMLPrettyPrintOptions>(rhs);
}

constexpr WhaleMLPrettyPrintOptions operator|(WhaleMLPrettyPrintOptions lhs, WhaleMLPrettyPrintOption rhs) {
    return lhs | static_cast<WhaleMLPrettyPrintOptions>(rhs);
}

constexpr bool hasWhaleMLPrettyPrintOption(WhaleMLPrettyPrintOptions options, WhaleMLPrettyPrintOption option) {
    return (options & static_cast<WhaleMLPrettyPrintOptions>(option)) != 0;
}

template <typename... Options>
constexpr WhaleMLPrettyPrintOptions makeWhaleMLPrettyPrintOptions(Options... options) {
    static_assert((std::is_convertible_v<Options, WhaleMLPrettyPrintOptions> && ...),
                  "WhaleML pretty print options must be WhaleMLPrettyPrintOption values or an option mask :/");
    return (0u | ... | static_cast<WhaleMLPrettyPrintOptions>(options));
}

struct WhaleMLNode {
public:
    WhaleMLNode() = default;
    WhaleMLNode(const WhaleMLNode& other) { copyFrom(other); }
    WhaleMLNode& operator=(const WhaleMLNode& other) { if (this != &other) copyFrom(other); return *this; }
    virtual ~WhaleMLNode() = default;
    inline void setParent(WhaleMLNode* parent, int level) { m_parent = parent; m_level = level; }
    inline int getLevel() const { return m_level; }
    inline WhaleMLNode* getParent() const { return m_parent; }
    inline std::unordered_map<std::string, std::unique_ptr<WhaleMLNode>>& getChildren() { return m_children; }
    inline const std::unordered_map<std::string, std::unique_ptr<WhaleMLNode>>& getChildren() const { return m_children; }
    inline const std::vector<std::string>& getChildOrder() const { return m_childOrder; }
    inline bool hasSerializationOrder() const { return m_hasSerializationOrder; }
    inline std::size_t getSerializationOrder() const { return m_serializationOrder; }
    inline WhaleMLPrettyPrintOptions getAutodetectedPrettyPrintOptions() const { return m_autodetectedPrettyPrintOptions; }
    inline void setAutodetectedPrettyPrintOptions(WhaleMLPrettyPrintOptions options) { m_autodetectedPrettyPrintOptions = options; }
    inline void setSerializationOrder(std::size_t order) {
        m_serializationOrder = order;
        m_hasSerializationOrder = true;
    }
    inline void ensureSerializationOrder() {
        if (!m_hasSerializationOrder) {
            m_serializationOrder = nextSerializationOrder();
            m_hasSerializationOrder = true;
        }
    }

    virtual bool isList() const = 0;
    inline bool isValue() const { return !isList(); }
    virtual std::unique_ptr<WhaleMLNode> clone() const = 0;

    WhaleMLNode* getChild(std::string_view key) {
        auto it = m_children.find(std::string(key));
        return it == m_children.end() ? nullptr : it->second.get();
    }

    const WhaleMLNode* getChild(std::string_view key) const {
        auto it = m_children.find(std::string(key));
        return it == m_children.end() ? nullptr : it->second.get();
    }

    bool setChild(std::string_view key, std::unique_ptr<WhaleMLNode> child) {
        if (!child) return false;
        const std::string keyString(key);
        auto existing = m_children.find(keyString);
        const bool overwritten = existing != m_children.end();
        if (!overwritten) {
            m_childOrder.push_back(keyString);
        }
        if (overwritten && existing->second && existing->second->hasSerializationOrder()) {
            child->setSerializationOrder(existing->second->getSerializationOrder());
        } else {
            child->ensureSerializationOrder();
        }
        child->setParent(this, m_level + 1);
        if (overwritten) {
            existing->second = std::move(child);
        } else {
            m_children.emplace(keyString, std::move(child));
        }
        return overwritten;
    }

    template <typename Fn>
    void forEachChildOrdered(Fn&& fn) const {
        std::size_t emitted = 0;
        for (const auto& key : m_childOrder) {
            auto it = m_children.find(key);
            if (it == m_children.end())
                continue;
            fn(it->first, it->second.get());
            ++emitted;
        }

        if (emitted == m_children.size())
            return;

        std::unordered_set<std::string> orderedKeys;
        orderedKeys.reserve(m_childOrder.size());
        for (const auto& key : m_childOrder) {
            orderedKeys.insert(key);
        }

        std::vector<std::string> extras;
        extras.reserve(m_children.size() - emitted);
        for (const auto& [key, child] : m_children) {
            (void)child;
            if (!orderedKeys.contains(key)) {
                extras.push_back(key);
            }
        }

        std::sort(extras.begin(), extras.end());
        for (const auto& key : extras) {
            auto it = m_children.find(key);
            if (it != m_children.end()) {
                fn(it->first, it->second.get());
            }
        }
    }

    void copyChildrenFrom(const WhaleMLNode& other) {
        m_children.clear();
        m_childOrder = other.m_childOrder;
        for (const auto& key : m_childOrder) {
            auto it = other.m_children.find(key);
            if (it == other.m_children.end() || !it->second)
                continue;
            auto cloned = it->second->clone();
            cloned->setParent(this, m_level + 1);
            m_children.emplace(key, std::move(cloned));
        }

        if (m_children.size() == other.m_children.size())
            return;

        std::unordered_set<std::string> copiedKeys;
        copiedKeys.reserve(m_childOrder.size());
        for (const auto& key : m_childOrder) {
            copiedKeys.insert(key);
        }

        std::vector<std::string> extras;
        extras.reserve(other.m_children.size() - m_children.size());
        for (const auto& [key, child] : other.m_children) {
            if (copiedKeys.contains(key) || !child)
                continue;
            extras.push_back(key);
        }

        std::sort(extras.begin(), extras.end());
        for (const auto& key : extras) {
            auto it = other.m_children.find(key);
            if (it == other.m_children.end() || !it->second)
                continue;
            auto cloned = it->second->clone();
            cloned->setParent(this, m_level + 1);
            m_children.emplace(key, std::move(cloned));
            m_childOrder.push_back(key);
        }
    }

private:
    void copyFrom(const WhaleMLNode& other) {
        m_parent = nullptr;
        m_level = other.m_level;
        m_serializationOrder = other.m_serializationOrder;
        m_hasSerializationOrder = other.m_hasSerializationOrder;
        m_autodetectedPrettyPrintOptions = other.m_autodetectedPrettyPrintOptions;
        m_children.clear();
        m_childOrder = other.m_childOrder;
        for (const auto& key : m_childOrder) {
            auto it = other.m_children.find(key);
            if (it == other.m_children.end() || !it->second)
                continue;
            auto cloned = it->second->clone();
            cloned->setParent(this, m_level + 1);
            m_children.emplace(key, std::move(cloned));
        }

        if (m_children.size() == other.m_children.size())
            return;

        std::unordered_set<std::string> copiedKeys;
        copiedKeys.reserve(m_childOrder.size());
        for (const auto& key : m_childOrder) {
            copiedKeys.insert(key);
        }

        std::vector<std::string> extras;
        extras.reserve(other.m_children.size() - m_children.size());
        for (const auto& [key, child] : other.m_children) {
            if (copiedKeys.contains(key) || !child)
                continue;
            extras.push_back(key);
        }

        std::sort(extras.begin(), extras.end());
        for (const auto& key : extras) {
            auto it = other.m_children.find(key);
            if (it == other.m_children.end() || !it->second)
                continue;
            auto cloned = it->second->clone();
            cloned->setParent(this, m_level + 1);
            m_children.emplace(key, std::move(cloned));
            m_childOrder.push_back(key);
        }
    }

    std::unordered_map<std::string, std::unique_ptr<WhaleMLNode>> m_children;
    std::vector<std::string> m_childOrder;
    WhaleMLNode* m_parent = nullptr;
    int m_level = 0;
    std::size_t m_serializationOrder = 0;
    WhaleMLPrettyPrintOptions m_autodetectedPrettyPrintOptions = 0;
    bool m_hasSerializationOrder = false;
    inline static std::size_t s_nextSerializationOrder = 1;

    static std::size_t nextSerializationOrder() {
        return s_nextSerializationOrder++;
    }
};

struct WhaleMLValue : public WhaleMLNode {
    WhaleMLValue() = default;
    WhaleMLValue(const WhaleMLValue& other) : WhaleMLNode(other), m_value(other.m_value) {}
    WhaleMLValue& operator=(const WhaleMLValue& other) {
        if (this != &other) {
            WhaleMLNode::operator=(other);
            m_value = other.m_value;
        }
        return *this;
    }

    WhaleMLScalarValue& getValue() { return m_value; }
    const WhaleMLScalarValue& getValue() const { return m_value; }
    void setValue(std::string_view value) { m_value.setValueInternal(value, true); }

    // Value assignment
    WhaleMLValue& operator=(WhaleMLScalarValue value) {
        m_value = value;
        return *this;
    }

    inline bool isList() const override { return false; }
    std::unique_ptr<WhaleMLNode> clone() const override { return std::make_unique<WhaleMLValue>(*this); }
private:
    WhaleMLScalarValue m_value;
};

struct WhaleMLList : public WhaleMLNode {
    WhaleMLList() = default;
    WhaleMLList(const WhaleMLList& other) : WhaleMLNode(other) {
        m_items.reserve(other.m_items.size());
        for (const auto& item : other.m_items) {
            if (item)
                m_items.push_back(std::make_unique<WhaleMLScalarValue>(*item));
            else
                m_items.push_back(nullptr);
        }
        m_sublists = other.m_sublists;
        m_explicitList = other.m_explicitList;
    }

    WhaleMLList& operator=(const WhaleMLList& other) {
        if (this != &other) {
            WhaleMLNode::operator=(other);
            m_items.clear();
            m_items.reserve(other.m_items.size());
            for (const auto& item : other.m_items) {
                if (item)
                    m_items.push_back(std::make_unique<WhaleMLScalarValue>(*item));
                else
                    m_items.push_back(nullptr);
            }
            m_sublists = other.m_sublists;
            m_explicitList = other.m_explicitList;
        }
        return *this;
    }

    std::vector<std::unique_ptr<WhaleMLScalarValue>>& getItems() { return m_items; }
    const std::vector<std::unique_ptr<WhaleMLScalarValue>>& getItems() const { return m_items; }

    std::map<int, WhaleMLList>& getSublists() { return m_sublists; }
    const std::map<int, WhaleMLList>& getSublists() const { return m_sublists; }
    void setExplicitList(bool explicitList = true) { m_explicitList = explicitList; }
    bool isExplicitList() const { return m_explicitList; }

    inline bool isList() const override { return true; }
    std::unique_ptr<WhaleMLNode> clone() const override { return std::make_unique<WhaleMLList>(*this); }
private:
    std::vector<std::unique_ptr<WhaleMLScalarValue>> m_items;
    std::map<int, WhaleMLList> m_sublists;
    bool m_explicitList = false;
};

class WhaleMLParser {
public:
    // Parses a WhaleML string (Does not accept filepaths!)
    static std::unique_ptr<WhaleMLNode> parse(std::string_view data);
    // Parses a WhaleML file and expands include "file.whaleml" before normal preprocessing.
    static std::unique_ptr<WhaleMLNode> parseFile(std::string_view filepath);
    // Serializes a WhaleML root node to string with default options
    static std::string serialize(const std::unique_ptr<WhaleMLNode>& root);
    // Serializes a WhaleML root node to string with custom options
    static std::string serialize(const std::unique_ptr<WhaleMLNode>& root, WhaleMLPrettyPrintOptions options);
};

