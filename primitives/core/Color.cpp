/*
 * File: Color.cpp
 *
 * Color primitive implementation.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Color.hpp"

#include <cctype>
#include <cstdio>
#include <stdexcept>

std::string Color::rgba() const {
    return "rgba(" + std::to_string(r) + "," + std::to_string(g) + "," +
        std::to_string(b) + "," + std::to_string(static_cast<float>(a) / 255.0f) + ")";
}

std::string Color::hex() const {
    char buf[10];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
    return std::string(buf);
}

Color Color::fromHex(std::string_view s) {
    auto trim = [](std::string_view& v) {
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.remove_prefix(1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())))  v.remove_suffix(1);
    };
    trim(s);
    if (s.empty()) throw std::invalid_argument("fromHex: empty string");
    if (s.front() == '#') s.remove_prefix(1);

    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    auto hexByte = [&](char hi, char lo) -> int {
        int h = hexVal(hi), l = hexVal(lo);
        if (h < 0 || l < 0) throw std::invalid_argument("fromHex: non-hex digit");
        return (h << 4) | l;
    };
    auto clamp255 = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };

    Color out{ 0, 0, 0, 255 };

    if (s.size() == 3 || s.size() == 4) {
        int rr = hexVal(s[0]), gg = hexVal(s[1]), bb = hexVal(s[2]);
        if (rr < 0 || gg < 0 || bb < 0) throw std::invalid_argument("fromHex: bad #RGB/#RGBA");
        rr = (rr << 4) | rr; gg = (gg << 4) | gg; bb = (bb << 4) | bb;
        int aa = 255;
        if (s.size() == 4) {
            int a1 = hexVal(s[3]);
            if (a1 < 0) throw std::invalid_argument("fromHex: bad alpha nibble");
            aa = (a1 << 4) | a1;
        }
        out = Color(clamp255(rr), clamp255(gg), clamp255(bb), clamp255(aa));
        return out;
    }

    if (s.size() == 6 || s.size() == 8) {
        int rr = hexByte(s[0], s[1]);
        int gg = hexByte(s[2], s[3]);
        int bb = hexByte(s[4], s[5]);
        int aa = 255;
        if (s.size() == 8) {
            aa = hexByte(s[6], s[7]);
        }
        out = Color(clamp255(rr), clamp255(gg), clamp255(bb), clamp255(aa));
        return out;
    }

    throw std::invalid_argument("fromHex: length must be 3,4,6,8 (without '#')");
}

