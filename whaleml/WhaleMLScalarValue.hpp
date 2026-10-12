/*
 * File: WhaleMLScalarValue.hpp
 *
 * Generic scalar value used by WhaleML parsing and query helpers.
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
#include <stdexcept>

class WhaleMLScalarValue {
public:
    inline WhaleMLScalarValue() {}
    inline WhaleMLScalarValue(std::string_view str, bool isValid, bool wasQuoted = false) : m_value(str), m_valid(isValid), m_wasQuoted(wasQuoted) {}
    inline bool isValid() const { return m_valid; }
    inline bool wasQuoted() const { return m_wasQuoted; }
    inline void setWasQuoted(bool wasQuoted) { m_wasQuoted = wasQuoted; }
    inline std::string asString() const { if (!m_valid) throw std::runtime_error("Invalid value access"); return m_value; }
    inline bool asBool() const { if (!m_valid) throw std::runtime_error("Invalid bool conversion"); if (m_value == "1" || m_value == "true") return true; if (m_value == "0" || m_value == "false") return false; throw std::runtime_error("Cannot convert to bool: " + m_value); }
    inline int asInt() const { if (!m_valid) throw std::runtime_error("Invalid int conversion"); try { return std::stoi(m_value); } catch (...) { throw std::runtime_error("Cannot convert to int: " + m_value); } }
    inline float asFloat() const { if (!m_valid) throw std::runtime_error("Invalid float conversion"); try { return std::stof(m_value); } catch (...) { throw std::runtime_error("Cannot convert to float: " + m_value); } }
    inline double asDouble() const { if (!m_valid) throw std::runtime_error("Invalid double conversion"); try { return std::stod(m_value); } catch (...) { throw std::runtime_error("Cannot convert to double: " + m_value); } }
    inline void setValueInternal(std::string_view value, bool valid, bool wasQuoted = false) { m_value = value; m_valid = valid; m_wasQuoted = wasQuoted; }
private:
    bool m_valid = false;
    bool m_wasQuoted = false;
    std::string m_value;
};

