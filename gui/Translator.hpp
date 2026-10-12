/*
 * File: Translator.hpp
 * 
 * A simple translation manager
 * 
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <boost/locale.hpp>
#include <locale>
#include <string>

#define _(args) boost::locale::translate(args).str().c_str()

class Translator {
public:
    static inline boost::locale::generator& getTranslatorGenerator() {
        static boost::locale::generator gen;
        return gen;
    }

    static inline void startTranslator(std::string_view path) {
        auto& gen = getTranslatorGenerator();
        gen.add_messages_path(std::string(path));
        gen.add_messages_domain("messages");
        std::locale::global(std::locale("C"));
    }

    static inline void setLanguage(std::string_view language) {
        auto& gen = getTranslatorGenerator();
        std::locale::global(gen(std::string(language)));
    }
};

