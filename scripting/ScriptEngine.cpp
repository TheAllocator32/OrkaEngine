#ifndef ORKA_DISABLE_SCRIPTING
/*
 * File: ScriptEngine.cpp
 *
 * Implementation of ScriptEngine.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "ScriptEngine.hpp"
#include "../essentials/Logger.hpp"
#include "../utilities/Tools.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ScriptBindings {
    void registerAngelScriptAddons(asIScriptEngine* engine);
}

static int luaChunkWriter(lua_State*, const void* p, size_t size, void* ud) {
    if (!p || !ud || size == 0) return 0;
    auto* out = static_cast<std::vector<char>*>(ud);
    const auto* bytes = static_cast<const char*>(p);
    out->insert(out->end(), bytes, bytes + size);
    return 0;
}

static void configureLuaState(lua_State* L) {
    if (L == nullptr) return;
    luaL_openlibs(L);
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const int n = lua_gettop(L);
        std::string msg;
        msg.reserve(128);
        for (int i = 1; i <= n; ++i) {
            size_t len = 0;
            const char* s = luaL_tolstring(L, i, &len);
            msg.append(s, len);
            lua_pop(L, 1);
            if (i < n) msg.push_back('\t');
        }
        Logger::log(LET_SPECIAL, "Script", msg.c_str());
        return 0;
    });
    lua_setglobal(L, "print");
}

static void angelMessageCallback(const asSMessageInfo* msg, void*) {
    if (msg == nullptr) return;
    LOG_ERROR_TYPE type = LET_INFO;
    if (msg->type == asMSGTYPE_ERROR) type = LET_ERROR;
    else if (msg->type == asMSGTYPE_WARNING) type = LET_WARN;

    Logger::log(type, "Orka, AngelScript",
                std::string(msg->section ? msg->section : "<script>") + ":" + std::to_string(msg->row) + ":" +
                std::to_string(msg->col) + " " + (msg->message ? msg->message : ""));
}

static void angelPrintInt(int value) {
    Logger::log(LET_SPECIAL, "Script", std::to_string(value));
}

static void angelPrintInt64(int64_t value) {
    Logger::log(LET_SPECIAL, "Script", std::to_string(value));
}

static void angelPrintFloat(float value) {
    Logger::log(LET_SPECIAL, "Script", std::to_string(value));
}

static void angelPrintDouble(double value) {
    Logger::log(LET_SPECIAL, "Script", std::to_string(value));
}

static void angelPrintBool(bool value) {
    Logger::log(LET_SPECIAL, "Script", value ? "true" : "false");
}

static void angelPrintString(const std::string& value) {
    Logger::log(LET_SPECIAL, "Script", value);
}

static bool configureAngelScriptEngine(asIScriptEngine* engine) {
    if (engine == nullptr) return false;
    bool ok = true;
    ok = engine->SetMessageCallback(asFUNCTION(angelMessageCallback), nullptr, asCALL_CDECL) >= 0 && ok;
    ScriptBindings::registerAngelScriptAddons(engine);
    ok = engine->RegisterGlobalFunction("void print(int)", asFUNCTION(angelPrintInt), asCALL_CDECL) >= 0 && ok;
    ok = engine->RegisterGlobalFunction("void print(int64)", asFUNCTION(angelPrintInt64), asCALL_CDECL) >= 0 && ok;
    ok = engine->RegisterGlobalFunction("void print(float)", asFUNCTION(angelPrintFloat), asCALL_CDECL) >= 0 && ok;
    ok = engine->RegisterGlobalFunction("void print(double)", asFUNCTION(angelPrintDouble), asCALL_CDECL) >= 0 && ok;
    ok = engine->RegisterGlobalFunction("void print(bool)", asFUNCTION(angelPrintBool), asCALL_CDECL) >= 0 && ok;
    ok = engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTIONPR(angelPrintString, (const std::string&), void), asCALL_CDECL) >= 0 && ok;
    return ok;
}

static bool readTextFile(std::string_view path, std::string& out, std::string& error) {
    out.clear();
    error.clear();

    std::ifstream file{ std::string(path), std::ios::binary };
    if (!file) {
        error = "failed to open file";
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    if (file.bad()) {
        error = "failed while reading file";
        return false;
    }
    return true;
}

static bool isAngelScriptPath(std::string_view path) {
    std::string p(path);
    std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return p.ends_with(".as") || p.ends_with(".angelscript") || p.ends_with(".angel");
}

static bool compileScriptChunk(std::string_view path, std::vector<char>& bytecode, std::string& compileError) {
    bytecode.clear();
    compileError.clear();

    lua_State* compiler = luaL_newstate();
    if (compiler == nullptr) {
        compileError = "luaL_newstate failed while compiling script";
        return false;
    }

    const std::string pathString(path);
    if (luaL_loadfile(compiler, pathString.c_str()) != LUA_OK) {
        compileError = lua_tostring(compiler, -1);
        lua_close(compiler);
        return false;
    }

    if (lua_dump(compiler, luaChunkWriter, &bytecode, 0) != LUA_OK) {
        compileError = "lua_dump failed while caching compiled script";
        lua_close(compiler);
        return false;
    }

    lua_close(compiler);
    return true;
}

static std::string threadIdToString(std::thread::id id) {
    std::ostringstream oss;
    oss << id;
    return oss.str();
}

bool ScriptEngine::ensureScriptThreadOrLog(Identifier scriptId, std::string_view action) {
    const auto ownerIt = sm_scriptThreadById.find(scriptId);
    if (ownerIt == sm_scriptThreadById.end()) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Invalid script id " + std::to_string(scriptId.getValue()) + " during '" + std::string(action) + "'");
        return false;
    }
    const std::thread::id currentThread = std::this_thread::get_id();
    if (ownerIt->second != currentThread) {
        Logger::log(LET_CRITICAL_ERROR, "Orka, Script Engine",
                    "Thread ownership violation for script id " + std::to_string(scriptId.getValue()) + " during '" + std::string(action) +
                    "' (owner=" + threadIdToString(ownerIt->second) + ", current=" + threadIdToString(currentThread) + ")");
        return false;
    }
    return true;
}

void ScriptEngine::init() {
    if (sm_initialized) return;
    sm_bindingProfiles.try_emplace("none");
    sm_initialized = true;
}

void ScriptEngine::registerBindingProfile(std::string_view name, void(*callback)()) {
    init();
    BindingProfile& profile = sm_bindingProfiles[std::string(name)];
    profile.m_callback = callback;
    profile.m_registered = false;
    profile.m_functions.clear();
    profile.m_variables.clear();
    profile.m_classes.clear();
    profile.m_functionNames.clear();
    profile.m_variableNames.clear();
    profile.m_classNames.clear();
}

void ScriptEngine::setDefaultBindingProfile(std::string_view name) {
    init();
    sm_defaultBindingProfile = std::string(name);
}

void ScriptEngine::deinit() {
    for (auto& kv : sm_states) { lua_close(kv.second); }
    for (auto& kv : sm_angelEngines) { kv.second->ShutDownAndRelease(); }
    sm_states.clear();
    sm_angelEngines.clear();
    sm_angelModules.clear();
    sm_scriptRefCounts.clear();
    sm_scriptPathById.clear();
    sm_scriptLanguageById.clear();
    sm_scriptThreadById.clear();
    sm_sharedScriptsByPath.clear();
    sm_compiledChunkCache.clear();
    sm_nextScriptID = 1;
    sm_functions.clear();
    sm_variables.clear();
    sm_classes.clear();
    sm_typeToClassName.clear();
    sm_angelOwnedObjects.clear();
    sm_angelSharedKeepAlive.clear();
    smt_buildingClass.reset();
    sm_defaultBindingProfile = "none";
    sm_bindingProfiles.clear();
    sm_currentBindingProfile = nullptr;
    sm_initialized = false;
}

Identifier ScriptEngine::loadScript(std::string_view path) {
    return loadScript(path, bindingProfileList(sm_defaultBindingProfile));
}

Identifier ScriptEngine::loadScript(std::string_view path, std::string_view bindingProfile) {
    return loadScript(path, bindingProfileList(bindingProfile));
}

Identifier ScriptEngine::loadScript(std::string_view path, std::initializer_list<std::string_view> bindingProfiles) {
    return loadScript(path, bindingProfileList(bindingProfiles));
}

Identifier ScriptEngine::loadScript(std::string_view path, const std::vector<std::string_view>& bindingProfiles) {
    return loadScript(path, bindingProfileList(bindingProfiles));
}

Identifier ScriptEngine::loadScript(std::string_view path, const std::vector<std::string>& bindingProfiles) {
    init();
    const std::vector<std::string> profiles = bindingProfileList(bindingProfiles);
    ensureBindingProfilesRegistered(profiles);
    const std::string pathString(path);
    if (isAngelScriptPath(pathString)) {
#ifdef ORKA_DISABLE_ANGELSCRIPT
        Logger::log(LET_ERROR, "Orka, Script Engine", "AngelScript is disabled by ORKA_DISABLE_ANGELSCRIPT.");
        return Identifier::INVALID;
#else
        Logger::log(LET_INFO, "Orka, Script Engine", "Loading AngelScript '" + MiscTools::getFilename(path) + "'...");

        std::string source;
        std::string readError;
        if (!readTextFile(pathString, source, readError)) {
            Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to read AngelScript file '" + MiscTools::getFilename(path) + "' \n\t" + readError);
            return Identifier::INVALID;
        }

        asIScriptEngine* engine = asCreateScriptEngine();
        if (engine == nullptr) {
            Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to create AngelScript engine for script '" + MiscTools::getFilename(path) + "'");
            return Identifier::INVALID;
        }
        if (!configureAngelScriptEngine(engine)) {
            Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to configure AngelScript engine for script '" + MiscTools::getFilename(path) + "'");
            engine->ShutDownAndRelease();
            return Identifier::INVALID;
        }

        exposeAngelProfiles(engine, profiles);

        asIScriptModule* module = engine->GetModule(pathString.c_str(), asGM_ALWAYS_CREATE);
        if (module == nullptr) {
            Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to create AngelScript module for script '" + MiscTools::getFilename(path) + "'");
            engine->ShutDownAndRelease();
            return Identifier::INVALID;
        }
        if (module->AddScriptSection(pathString.c_str(), source.c_str(), source.size()) < 0 || module->Build() < 0) {
            Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to build AngelScript file '" + MiscTools::getFilename(path) + "'");
            engine->ShutDownAndRelease();
            return Identifier::INVALID;
        }

        const Identifier id = sm_nextScriptID++;
        sm_angelEngines.emplace(id, engine);
        sm_angelModules.emplace(id, module);
        sm_scriptRefCounts[id] = 1;
        sm_scriptPathById[id] = pathString;
        sm_scriptLanguageById[id] = ScriptLanguage::AngelScript;
        sm_scriptThreadById[id] = std::this_thread::get_id();
        return id;
#endif
    }

#ifdef ORKA_DISABLE_LUA
    Logger::log(LET_ERROR, "Orka, Script Engine", "Lua is disabled by ORKA_DISABLE_LUA.");
    return Identifier::INVALID;
#else
    Logger::log(LET_INFO, "Orka, Script Engine", "Loading Lua Script '" + MiscTools::getFilename(path) + "'...");

    auto& chunk = sm_compiledChunkCache[pathString];
    if (!chunk.m_valid) {
        if (!compileScriptChunk(pathString, chunk.m_bytecode, chunk.m_compileError)) {
            chunk.m_valid = false;
            Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to compile script file '" + MiscTools::getFilename(path) + "' \n\t" + chunk.m_compileError);
            return Identifier::INVALID;
        }
        chunk.m_valid = true;
    }
    if (!chunk.m_valid) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to compile script file '" + MiscTools::getFilename(path) + "' \n\t" + chunk.m_compileError);
        return Identifier::INVALID;
    }

    lua_State* L = luaL_newstate();
    if (L == nullptr) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to create Lua state for script '" + MiscTools::getFilename(path) + "'");
        return Identifier::INVALID;
    }
    configureLuaState(L);

    if (luaL_loadbuffer(L, chunk.m_bytecode.data(), chunk.m_bytecode.size(), pathString.c_str())) {
        std::string err = lua_tostring(L, -1);
        lua_close(L);
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to initialize script file '" + MiscTools::getFilename(path) + "' \n\t" + err);
        return Identifier::INVALID;
    }

    exposeLuaProfiles(L, profiles);

    if (lua_pcall(L, 0, 0, 0)) {
        std::string err = lua_tostring(L, -1);
        lua_close(L);
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to initialize script file '" + MiscTools::getFilename(path) + "' \n\t" + err);
        return Identifier::INVALID;
    }

    const Identifier id = sm_nextScriptID++;
    sm_states.emplace(id, L);
    sm_scriptRefCounts[id] = 1;
    sm_scriptPathById[id] = pathString;
    sm_scriptLanguageById[id] = ScriptLanguage::Lua;
    sm_scriptThreadById[id] = std::this_thread::get_id();
    return id;
#endif
}

Identifier ScriptEngine::loadSharedScript(std::string_view path) {
    return loadSharedScript(path, bindingProfileList(sm_defaultBindingProfile));
}

Identifier ScriptEngine::loadSharedScript(std::string_view path, std::string_view bindingProfile) {
    return loadSharedScript(path, bindingProfileList(bindingProfile));
}

Identifier ScriptEngine::loadSharedScript(std::string_view path, std::initializer_list<std::string_view> bindingProfiles) {
    return loadSharedScript(path, bindingProfileList(bindingProfiles));
}

Identifier ScriptEngine::loadSharedScript(std::string_view path, const std::vector<std::string_view>& bindingProfiles) {
    return loadSharedScript(path, bindingProfileList(bindingProfiles));
}

Identifier ScriptEngine::loadSharedScript(std::string_view path, const std::vector<std::string>& bindingProfiles) {
    init();
    const std::vector<std::string> profiles = bindingProfileList(bindingProfiles);
    ensureBindingProfilesRegistered(profiles);
    const std::string pathString(path);
    const std::string sharedKey = pathString + "#" + bindingProfileKey(profiles);
    auto sharedIt = sm_sharedScriptsByPath.find(sharedKey);
    if (sharedIt != sm_sharedScriptsByPath.end()) {
        const Identifier existingId = sharedIt->second;
        const bool hasLuaState = sm_states.contains(existingId);
        const bool hasAngelEngine = sm_angelEngines.contains(existingId);
        if (hasLuaState || hasAngelEngine) {
            if (!ensureScriptThreadOrLog(existingId, "loadSharedScript(reuse)")) {
                return Identifier::INVALID;
            }
            ++sm_scriptRefCounts[existingId];
            return existingId;
        }
        sm_sharedScriptsByPath.erase(sharedIt);
    }

    const Identifier scriptId = loadScript(pathString, profiles);
    if (scriptId == Identifier::INVALID) return Identifier::INVALID;
    sm_sharedScriptsByPath[sharedKey] = scriptId;
    return scriptId;
}

ScriptEngine::ScriptLanguage ScriptEngine::getScriptLanguage(Identifier scriptId) {
    auto it = sm_scriptLanguageById.find(scriptId);
    if (it == sm_scriptLanguageById.end()) {
        throw std::runtime_error("Invalid script id: " + std::to_string(scriptId.getValue()));
    }
    return it->second;
}

std::string ScriptEngine::getCurrentScriptPath() {
    if (smt_activeScriptID == Identifier::INVALID) return std::string();
    auto it = sm_scriptPathById.find(smt_activeScriptID);
    if (it == sm_scriptPathById.end()) return {};
    return it->second;
}

void ScriptEngine::unloadScript(Identifier scriptId) {
    if (!ensureScriptThreadOrLog(scriptId, "unloadScript")) return;
    auto refIt = sm_scriptRefCounts.find(scriptId);
    if (refIt != sm_scriptRefCounts.end()) {
        refIt->second -= 1;
        if (refIt->second > 0) {
            return;
        }
        sm_scriptRefCounts.erase(refIt);
    }

    auto pathIt = sm_scriptPathById.find(scriptId);
    if (pathIt != sm_scriptPathById.end()) {
        for (auto sharedIt = sm_sharedScriptsByPath.begin(); sharedIt != sm_sharedScriptsByPath.end();) {
            if (sharedIt->second == scriptId) sharedIt = sm_sharedScriptsByPath.erase(sharedIt);
            else ++sharedIt;
        }
        sm_scriptPathById.erase(pathIt);
    }

    auto it = sm_states.find(scriptId);
    if (it != sm_states.end()) {
        lua_close(it->second);
        sm_states.erase(it);
    }

    auto angelIt = sm_angelEngines.find(scriptId);
    if (angelIt != sm_angelEngines.end()) {
        angelIt->second->ShutDownAndRelease();
        sm_angelEngines.erase(angelIt);
        sm_angelModules.erase(scriptId);
    }

    sm_scriptLanguageById.erase(scriptId);
    sm_scriptThreadById.erase(scriptId);
}

bool ScriptEngine::exposeFunction(Identifier scriptId, std::string_view name) {
    if (name == "print") return false; // Block you from overriding the print function :)
    auto itF = sm_functions.find(std::string(name));
    if (itF == sm_functions.end()) return false;
    auto langIt = sm_scriptLanguageById.find(scriptId);
    if (langIt != sm_scriptLanguageById.end() && langIt->second == ScriptLanguage::AngelScript) {
        asIScriptEngine* engine = getAngelEngineOrNull(scriptId, "exposeFunction");
        return engine != nullptr && itF->second->pushToAngelScript(engine, name);
    }
    lua_State* L = getStateOrNull(scriptId, "exposeFunction");
    if (L == nullptr) return false;
    itF->second->pushToState(L, name);
    return true;
}

bool ScriptEngine::exposeVariable(Identifier scriptId, std::string_view name) {
    auto itV = sm_variables.find(std::string(name));
    if (itV == sm_variables.end()) return false;
    auto langIt = sm_scriptLanguageById.find(scriptId);
    if (langIt != sm_scriptLanguageById.end() && langIt->second == ScriptLanguage::AngelScript) {
        asIScriptEngine* engine = getAngelEngineOrNull(scriptId, "exposeVariable");
        return engine != nullptr && setAngelGlobal(engine, name, itV->second);
    }
    lua_State* L = getStateOrNull(scriptId, "exposeVariable");
    if (L == nullptr) return false;
    setGlobalVariant(L, name, itV->second);
    return true;
}

bool ScriptEngine::exposeClass(Identifier scriptId, std::string_view name) {
    auto itC = sm_classes.find(std::string(name));
    if (itC == sm_classes.end()) return false;
    auto langIt = sm_scriptLanguageById.find(scriptId);
    if (langIt != sm_scriptLanguageById.end() && langIt->second == ScriptLanguage::AngelScript) {
        asIScriptEngine* engine = getAngelEngineOrNull(scriptId, "exposeClass");
        if (engine == nullptr) return false;
        return engine->GetTypeInfoByName(std::string(name).c_str()) != nullptr;
    }
    lua_State* L = getStateOrNull(scriptId, "exposeClass");
    if (L == nullptr) return false;
    itC->second->pushToState(L);
    return true;
}

void ScriptEngine::exposeAll(Identifier scriptId) {
    auto langIt = sm_scriptLanguageById.find(scriptId);
    if (langIt != sm_scriptLanguageById.end() && langIt->second == ScriptLanguage::AngelScript) {
        asIScriptEngine* engine = getAngelEngineOrNull(scriptId, "exposeAll");
        if (engine != nullptr) exposeAngelProfile(engine, sm_defaultBindingProfile);
        return;
    }

    lua_State* L = getStateOrNull(scriptId, "exposeAll");
    if (L == nullptr) return;
    exposeLuaProfile(L, sm_defaultBindingProfile);
}

lua_State* ScriptEngine::getStateOrNull(Identifier scriptId, std::string_view action) {
    auto it = sm_states.find(scriptId);
    if (it == sm_states.end()) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Invalid script id " + std::to_string(scriptId.getValue()) + " during '" + std::string(action) + "'");
        return nullptr;
    }
    if (!ensureScriptThreadOrLog(scriptId, action)) return nullptr;
    return it->second;
}

asIScriptEngine* ScriptEngine::getAngelEngineOrNull(Identifier scriptId, std::string_view action) {
    auto it = sm_angelEngines.find(scriptId);
    if (it == sm_angelEngines.end()) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Invalid script id " + std::to_string(scriptId.getValue()) + " during '" + std::string(action) + "'");
        return nullptr;
    }
    if (!ensureScriptThreadOrLog(scriptId, action)) return nullptr;
    return it->second;
}

asIScriptModule* ScriptEngine::getAngelModuleOrNull(Identifier scriptId, std::string_view action) {
    auto it = sm_angelModules.find(scriptId);
    if (it == sm_angelModules.end() || it->second == nullptr) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Invalid script id " + std::to_string(scriptId.getValue()) + " during '" + std::string(action) + "'");
        return nullptr;
    }
    return it->second;
}

void ScriptEngine::failScriptAction(std::string_view message) {
    Logger::log(LET_ERROR, "Orka, Script Engine", std::string(message));
    throw std::runtime_error(std::string(message));
}

void ScriptEngine::setGlobalVariant(lua_State* L, std::string_view name, const RegisteredVariable& v) {
    const std::string nameString(name);
    if (v.m_kind == RegisteredVariable::Kind::Value) {
        if (std::holds_alternative<bool>(v.m_value)) lua_pushboolean(L, std::get<bool>(v.m_value));
        else if (std::holds_alternative<int64_t>(v.m_value)) lua_pushinteger(L, static_cast<lua_Integer>(std::get<int64_t>(v.m_value)));
        else if (std::holds_alternative<double>(v.m_value)) lua_pushnumber(L, static_cast<lua_Number>(std::get<double>(v.m_value)));
        else if (std::holds_alternative<std::string>(v.m_value)) { const auto& s = std::get<std::string>(v.m_value); lua_pushlstring(L, s.c_str(), s.size()); } else lua_pushnil(L);
        lua_setglobal(L, nameString.c_str());
    } else {
        if (v.m_type == typeid(bool)) { bool val = *static_cast<bool*>(v.m_ref); lua_pushboolean(L, val); } else if (v.m_type == typeid(int)) { int val = *static_cast<int*>(v.m_ref); lua_pushinteger(L, static_cast<lua_Integer>(val)); } else if (v.m_type == typeid(int64_t)) { int64_t val = *static_cast<int64_t*>(v.m_ref); lua_pushinteger(L, static_cast<lua_Integer>(val)); } else if (v.m_type == typeid(double)) { double val = *static_cast<double*>(v.m_ref); lua_pushnumber(L, static_cast<lua_Number>(val)); } else if (v.m_type == typeid(std::string)) { auto& s = *static_cast<std::string*>(v.m_ref); lua_pushlstring(L, s.c_str(), s.size()); } else lua_pushnil(L);
        lua_setglobal(L, nameString.c_str());
    }
}

bool ScriptEngine::setAngelGlobal(asIScriptEngine* engine, std::string_view name, RegisteredVariable& v) {
    if (engine == nullptr) return false;
    const std::string nameString(name);
    std::string typeName;
    void* ptr = nullptr;

    if (v.m_kind == RegisteredVariable::Kind::Ref) {
        ptr = v.m_ref;
    } else if (v.m_type == typeid(bool)) {
        v.m_boolStorage = std::get<bool>(v.m_value);
        ptr = &v.m_boolStorage;
    } else if (v.m_type == typeid(int)) {
        v.m_intStorage = static_cast<int>(std::get<int64_t>(v.m_value));
        ptr = &v.m_intStorage;
    } else if (v.m_type == typeid(int64_t)) {
        v.m_int64Storage = std::get<int64_t>(v.m_value);
        ptr = &v.m_int64Storage;
    } else if (v.m_type == typeid(float)) {
        v.m_floatStorage = static_cast<float>(std::get<double>(v.m_value));
        ptr = &v.m_floatStorage;
    } else if (v.m_type == typeid(double)) {
        v.m_doubleStorage = std::get<double>(v.m_value);
        ptr = &v.m_doubleStorage;
    } else if (v.m_type == typeid(std::string) || v.m_type == typeid(const char*)) {
        v.m_stringStorage = std::get<std::string>(v.m_value);
        ptr = &v.m_stringStorage;
    } else {
        auto classIt = sm_typeToClassName.find(v.m_type);
        if (classIt != sm_typeToClassName.end() && v.m_kind == RegisteredVariable::Kind::Ref) {
            v.m_angelObjectHandleStorage = v.m_ref;
            ptr = &v.m_angelObjectHandleStorage;
        }
    }

    if (v.m_type == typeid(bool)) typeName = "bool";
    else if (v.m_type == typeid(int)) typeName = "int";
    else if (v.m_type == typeid(int64_t)) typeName = "int64";
    else if (v.m_type == typeid(float)) typeName = "float";
    else if (v.m_type == typeid(double)) typeName = "double";
    else if (v.m_type == typeid(std::string) || v.m_type == typeid(const char*)) typeName = "string";
    else {
        auto classIt = sm_typeToClassName.find(v.m_type);
        if (classIt != sm_typeToClassName.end() && v.m_kind == RegisteredVariable::Kind::Ref) {
            typeName = classIt->second + "@";
        }
    }

    if (typeName.empty()) {
        Logger::log(LET_WARN, "Orka, Script Engine", "Unsupported AngelScript global type for: " + nameString);
        return false;
    }

    if (ptr == nullptr) {
        Logger::log(LET_WARN, "Orka, Script Engine", "Null AngelScript global storage for: " + nameString);
        return false;
    }

    const std::string decl = typeName + " " + nameString;
    const int rc = engine->RegisterGlobalProperty(decl.c_str(), ptr);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript global: " + decl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

void ScriptEngine::exposeAngelProfile(asIScriptEngine* engine, std::string_view bindingProfile) {
    if (engine == nullptr) return;
    auto profileIt = sm_bindingProfiles.find(std::string(bindingProfile));
    if (profileIt == sm_bindingProfiles.end()) return;
    BindingProfile& profile = profileIt->second;
    for (const std::string& name : profile.m_classes) {
        auto it = sm_classes.find(name);
        if (it != sm_classes.end()) it->second->registerAngelScriptType(engine);
    }
    for (const std::string& name : profile.m_classes) {
        auto it = sm_classes.find(name);
        if (it != sm_classes.end()) it->second->pushToAngelScript(engine);
    }
    for (const std::string& name : profile.m_functions) {
        auto it = sm_functions.find(name);
        if (it != sm_functions.end() && name != "print") it->second->pushToAngelScript(engine, name);
    }
    for (const std::string& name : profile.m_variables) {
        auto it = sm_variables.find(name);
        if (it != sm_variables.end()) setAngelGlobal(engine, name, it->second);
    }
}

void ScriptEngine::exposeAngelProfiles(asIScriptEngine* engine, const std::vector<std::string>& bindingProfiles) {
    if (engine == nullptr) return;
    std::vector<BindingProfile*> profiles;
    profiles.reserve(bindingProfiles.size());
    for (const std::string& bindingProfile : bindingProfiles) {
        auto profileIt = sm_bindingProfiles.find(bindingProfile);
        if (profileIt != sm_bindingProfiles.end()) profiles.push_back(&profileIt->second);
    }

    for (BindingProfile* profile : profiles) {
        for (const std::string& name : profile->m_classes) {
            auto it = sm_classes.find(name);
            if (it != sm_classes.end()) it->second->registerAngelScriptType(engine);
        }
    }
    for (BindingProfile* profile : profiles) {
        for (const std::string& name : profile->m_classes) {
            auto it = sm_classes.find(name);
            if (it != sm_classes.end()) it->second->pushToAngelScript(engine);
        }
    }
    for (BindingProfile* profile : profiles) {
        for (const std::string& name : profile->m_functions) {
            auto it = sm_functions.find(name);
            if (it != sm_functions.end() && name != "print") it->second->pushToAngelScript(engine, name);
        }
    }
    for (BindingProfile* profile : profiles) {
        for (const std::string& name : profile->m_variables) {
            auto it = sm_variables.find(name);
            if (it != sm_variables.end()) setAngelGlobal(engine, name, it->second);
        }
    }
}

void ScriptEngine::exposeLuaProfile(lua_State* L, std::string_view bindingProfile) {
    if (L == nullptr) return;
    auto profileIt = sm_bindingProfiles.find(std::string(bindingProfile));
    if (profileIt == sm_bindingProfiles.end()) return;
    BindingProfile& profile = profileIt->second;
    for (const std::string& name : profile.m_functions) {
        auto it = sm_functions.find(name);
        if (it != sm_functions.end()) it->second->pushToState(L, name);
    }
    for (const std::string& name : profile.m_variables) {
        auto it = sm_variables.find(name);
        if (it != sm_variables.end()) setGlobalVariant(L, name, it->second);
    }
    for (const std::string& name : profile.m_classes) {
        auto it = sm_classes.find(name);
        if (it != sm_classes.end()) it->second->pushToState(L);
    }
}

void ScriptEngine::exposeLuaProfiles(lua_State* L, const std::vector<std::string>& bindingProfiles) {
    if (L == nullptr) return;
    for (const std::string& bindingProfile : bindingProfiles) {
        exposeLuaProfile(L, bindingProfile);
    }
}

void ScriptEngine::ensureBindingProfileRegistered(std::string_view bindingProfile) {
    init();
    auto profileIt = sm_bindingProfiles.find(std::string(bindingProfile));
    if (profileIt == sm_bindingProfiles.end()) {
        Logger::log(LET_WARN, "Orka, Script Engine", "Unknown binding profile '" + std::string(bindingProfile) + "'. Using no automatic bindings.");
        return;
    }
    BindingProfile& profile = profileIt->second;
    if (profile.m_registered || profile.m_callback == nullptr) return;
    BindingProfile* previousProfile = sm_currentBindingProfile;
    sm_currentBindingProfile = &profile;
    profile.m_callback();
    sm_currentBindingProfile = previousProfile;
    profile.m_registered = true;
}

void ScriptEngine::ensureBindingProfilesRegistered(const std::vector<std::string>& bindingProfiles) {
    for (const std::string& bindingProfile : bindingProfiles) {
        ensureBindingProfileRegistered(bindingProfile);
    }
}

std::vector<std::string> ScriptEngine::bindingProfileList(std::string_view bindingProfile) {
    return { std::string(bindingProfile) };
}

std::vector<std::string> ScriptEngine::bindingProfileList(std::initializer_list<std::string_view> bindingProfiles) {
    std::vector<std::string> out;
    out.reserve(bindingProfiles.size());
    for (std::string_view bindingProfile : bindingProfiles) {
        out.emplace_back(bindingProfile);
    }
    return out;
}

std::vector<std::string> ScriptEngine::bindingProfileList(const std::vector<std::string>& bindingProfiles) {
    return bindingProfiles;
}

std::vector<std::string> ScriptEngine::bindingProfileList(const std::vector<std::string_view>& bindingProfiles) {
    std::vector<std::string> out;
    out.reserve(bindingProfiles.size());
    for (std::string_view bindingProfile : bindingProfiles) {
        out.emplace_back(bindingProfile);
    }
    return out;
}

std::string ScriptEngine::bindingProfileKey(const std::vector<std::string>& bindingProfiles) {
    std::string out;
    for (const std::string& bindingProfile : bindingProfiles) {
        if (!out.empty()) out += "+";
        out += bindingProfile;
    }
    return out;
}

void ScriptEngine::rememberProfileSymbol(std::string_view name, std::vector<std::string>& orderedNames, std::unordered_set<std::string>& knownNames) {
    const std::string nameString(name);
    if (knownNames.insert(nameString).second) {
        orderedNames.push_back(nameString);
    }
}

#endif

