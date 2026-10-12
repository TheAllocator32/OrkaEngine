#ifndef ORKA_DISABLE_SCRIPTING
/*
 * File: ext_AngelscriptAddonsImpl.cpp
 *
 * AngelScript standard add-on implementation and registration.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Bindings.hpp"

#include <angelscript/scriptany/scriptany.h>
#include <angelscript/scriptarray/scriptarray.h>
#include <angelscript/scriptdictionary/scriptdictionary.h>
#include <angelscript/scripthandle/scripthandle.h>
#include <angelscript/scriptmath/scriptmath.h>
#include <angelscript/scriptmath/scriptmathcomplex.h>
#include <angelscript/scriptstdstring/scriptstdstring.h>
#include <angelscript/weakref/weakref.h>

#include <angelscript/scriptstdstring/scriptstdstring.cpp>
#include <angelscript/scriptarray/scriptarray.cpp>
#include <angelscript/scriptstdstring/scriptstdstring_utils.cpp>
#include <angelscript/scriptany/scriptany.cpp>
#include <angelscript/scriptdictionary/scriptdictionary.cpp>
#include <angelscript/scripthandle/scripthandle.cpp>
#include <angelscript/weakref/weakref.cpp>
#include <angelscript/scriptmath/scriptmath.cpp>
#include <angelscript/scriptmath/scriptmathcomplex.cpp>

namespace ScriptBindings {
    void registerAngelScriptAddons(asIScriptEngine* engine) {
        if (engine == nullptr) return;

        RegisterStdString(engine);
        RegisterScriptArray(engine, true);
        RegisterStdStringUtils(engine);
        RegisterScriptAny(engine);
        RegisterScriptDictionary(engine);
        RegisterScriptHandle(engine);
        RegisterScriptWeakRef(engine);
        RegisterScriptMath(engine);
        RegisterScriptMathComplex(engine);
    }
}

#endif

