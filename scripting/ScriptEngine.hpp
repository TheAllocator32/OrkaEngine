/*
 * File: ScriptEngine.hpp
 *
 * "Lightweight" static Lua/AngelScript wrapper with registration/expose API for globals, functions, and classes.
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../essentials/Identifier.hpp"
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <angelscript.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

/*
 * Important info / anti confusion:
 * ScriptEngine IS threadsafe!!!
 * BUT: All things happen THREAD-LOCALLY! So don't expect Orka to accept you modifying a SceneNode script from your GameMode!
 * Reason: Lua states are not thread save and mutex based lock would be a performance nightmare.
 * In addition, registration/global registries are process-global and should be configured during initialization before scripts are used concurrently.
 *
 * Furthermore, AngelScript supports function overloading (Lua doesn't!!) so if you want to overload a function I would recommend this pattern:
 *   ScriptEngine::addMethod<SceneNode, void, const vector3df&>("setPosition", &SceneNode::setPosition);
 *   ScriptEngine::addMethodAngelScript<SceneNode, void, double, double, double>("setPosition", &SceneNode::setPosition);
 *   ScriptEngine::addMethodLua<SceneNode, void, double, double, double>("setPositionN", &sceneNodeSetPositionN); // Here you have to manually define sceneNodeSetPositionN
 */

class ScriptEngine {
public:
    using Variant = std::variant<bool, int64_t, double, std::string>;
    enum class ScriptLanguage { Lua, AngelScript };

    // Marks a returned class pointer as owned-by-Lua (deleted in __gc).
    template<typename T>
    struct AdoptPtr { T* ptr = nullptr; };

    // Initialize internal registries.
    static void init();
    // Deinitialize all scripts and clear registries.
    static void deinit();
    // Register a named binding profile. The callback may call addGlobalFunction/addGlobalVariable/beginClass/etc.
    static void registerBindingProfile(std::string_view name, void(*callback)());
    // Select the profile used by loadScript(path) and loadSharedScript(path).
    static void setDefaultBindingProfile(std::string_view name);

    // Load and run a Lua/Angelscript (depending on extension .as/.lua) file in a fresh state; returns script id.
    static Identifier loadScript(std::string_view path);
    static Identifier loadScript(std::string_view path, std::string_view bindingProfile);
    static Identifier loadScript(std::string_view path, std::initializer_list<std::string_view> bindingProfiles);
    static Identifier loadScript(std::string_view path, const std::vector<std::string>& bindingProfiles);
    static Identifier loadScript(std::string_view path, const std::vector<std::string_view>& bindingProfiles);
    // Load or reuse a shared Lua/AngelScript (depending on extension .as/.lua) state for this script path; returns script id. 
    static Identifier loadSharedScript(std::string_view path);
    static Identifier loadSharedScript(std::string_view path, std::string_view bindingProfile);
    static Identifier loadSharedScript(std::string_view path, std::initializer_list<std::string_view> bindingProfiles);
    static Identifier loadSharedScript(std::string_view path, const std::vector<std::string>& bindingProfiles);
    static Identifier loadSharedScript(std::string_view path, const std::vector<std::string_view>& bindingProfiles);
    // Returns the runtime assigned to a loaded script id.
    static ScriptLanguage getScriptLanguage(Identifier scriptId);
    // Returns the currently executing script path on this thread, if any.
    static std::string getCurrentScriptPath();
    // Destroy a script state by id.
    static void unloadScript(Identifier scriptId);

    // Register a global variable by value (copied at expose time).
    template<typename T>
    static void addGlobalVariable(std::string_view name, const T& value);
    // Register a global variable by reference (refreshed at expose time). Pointer must remain valid for the engine lifetime or until registry clear!!
    template<typename T>
    static void addGlobalVariableRef(std::string_view name, T* ptr);
    // Register a free function as a global (supports class pointers in args/return).
    template<typename Ret, typename... Args>
    static void addGlobalFunction(std::string_view name, Ret(*func)(Args...));

    // Registers a class/type
    template<typename T>
    static void beginClass(std::string_view name);
    // Adds a Constructor
    template<typename T, typename... CtorArgs>
    static void addConstructor();
    // Adds a zero-Lua-arg constructor with fixed compile-time C++ arguments.
    template<typename T, auto FixedArg, auto... FixedArgs>
    static void addConstructor();
    // Adds a constructor via a custom factory function.
    template<typename T, typename Ret, typename... Args>
    static void addConstructor(Ret(*factory)(Args...));
    // Adds method
    template<typename T, typename Ret, typename... Args>
    static void addMethod(std::string_view name, Ret(T::* method)(Args...));
    template<typename T, typename Ret, typename... Args>
    static void addMethodLua(std::string_view name, Ret(T::* method)(Args...));
    template<typename T, typename Ret, typename... Args>
    static void addMethodAngelScript(std::string_view name, Ret(T::* method)(Args...));
    // Adds method via free-function wrapper receiving the instance as first argument.
    template<typename T, typename Ret, typename... Args>
    static void addMethod(std::string_view name, Ret(*func)(T*, Args...));
    template<typename T, typename Ret, typename... Args>
    static void addMethodLua(std::string_view name, Ret(*func)(T*, Args...));
    template<typename T, typename Ret, typename... Args>
    static void addMethodAngelScript(std::string_view name, Ret(*func)(T*, Args...));
    // Adds constant method
    template<typename T, typename Ret, typename... Args>
    static void addConstMethod(std::string_view name, Ret(T::* method)(Args...) const);
    template<typename T, typename Ret, typename... Args>
    static void addConstMethodLua(std::string_view name, Ret(T::* method)(Args...) const);
    template<typename T, typename Ret, typename... Args>
    static void addConstMethodAngelScript(std::string_view name, Ret(T::* method)(Args...) const);
    // Adds constant method via free-function wrapper receiving the instance as first argument
    template<typename T, typename Ret, typename... Args>
    static void addConstMethod(std::string_view name, Ret(*func)(const T*, Args...));
    template<typename T, typename Ret, typename... Args>
    static void addConstMethodLua(std::string_view name, Ret(*func)(const T*, Args...));
    template<typename T, typename Ret, typename... Args>
    static void addConstMethodAngelScript(std::string_view name, Ret(*func)(const T*, Args...));
    // Adds member variable
    template<typename T, typename Field>
    static void addProperty(std::string_view name, Field T::* field);
    // End of class definition
    template<typename T>
    static void endClass();

    // Turn a function pointer Ret(*)(Args...) with Ret = T* into AdoptPtr<T>(Args...).
    template<auto Fn>
    static constexpr auto adoptReturn();

    // Expose registered symbol into a specific script state.
    static bool exposeFunction(Identifier scriptId, std::string_view name);
    static bool exposeVariable(Identifier scriptId, std::string_view name);
    static bool exposeClass(Identifier scriptId, std::string_view name);
    // Expose everything registered so far into a script state.
    static void exposeAll(Identifier scriptId);

    // Invoke a global Lua function in the script and return Ret.
    template<typename Ret = void, typename... Args>
    static Ret invoke(Identifier scriptId, std::string_view functionName, Args... args);

private:
    struct RegisteredFunctionBase {
        virtual ~RegisteredFunctionBase() = default;
        virtual void pushToState(lua_State* L, std::string_view name) = 0;
        virtual bool pushToAngelScript(asIScriptEngine* engine, std::string_view name) = 0;
    };

    template<typename Ret, typename... Args>
    struct RegisteredFunction final : RegisteredFunctionBase {
        std::function<Ret(Args...)> m_func;
        explicit RegisteredFunction(std::function<Ret(Args...)> fn) : m_func(std::move(fn)) {}
        static int thunk(lua_State* L);
        static void angelGeneric(asIScriptGeneric* gen);
        void pushToState(lua_State* L, std::string_view name) override {
            lua_pushlightuserdata(L, this);
            lua_pushcclosure(L, &RegisteredFunction::thunk, 1);
            lua_setglobal(L, std::string(name).c_str());
        }
        bool pushToAngelScript(asIScriptEngine* engine, std::string_view name) override;
    };

    struct RegisteredVariable {
        enum class Kind { Value, Ref };
        Kind m_kind{ Kind::Value };
        Variant m_value{};
        void* m_ref{ nullptr };
        std::type_index m_type{ typeid(void) };
        bool m_boolStorage = false;
        int m_intStorage = 0;
        int64_t m_int64Storage = 0;
        float m_floatStorage = 0.0f;
        double m_doubleStorage = 0.0;
        std::string m_stringStorage;
        void* m_angelObjectHandleStorage = nullptr;
    };

    struct RegisteredClassBase {
        virtual ~RegisteredClassBase() = default;
        virtual void pushToState(lua_State* L) = 0;
        virtual bool registerAngelScriptType(asIScriptEngine* engine) = 0;
        virtual bool pushToAngelScript(asIScriptEngine* engine) = 0;
        std::string m_name;
        std::string m_metaName;
    };

    template<typename T>
    struct UserdataBox {
        T* ptr = nullptr;
        bool owned = false;
        std::shared_ptr<T> gcPtr = nullptr;
    };

    template<typename T>
    struct RegisteredClass final : RegisteredClassBase {
        using Self = RegisteredClass<T>;

        struct MethodBase {
            virtual ~MethodBase() = default;
            virtual std::string_view name() const = 0;
            virtual void bind(lua_State* L, Self* owner) = 0;
            virtual bool bindAngelScript(asIScriptEngine* engine, std::string_view className) = 0;
        };

        template<typename Ret, typename... Args>
        struct Method : MethodBase {
            std::string m_name;
            std::function<Ret(T*, Args...)> m_fn;
            bool m_bindLua = true;
            bool m_bindAngelScript = true;
            explicit Method(std::string_view n, std::function<Ret(T*, Args...)> f, bool bindLua = true, bool bindAngelScript = true)
                : m_name(std::move(n)), m_fn(std::move(f)), m_bindLua(bindLua), m_bindAngelScript(bindAngelScript) {
            }
            std::string_view name() const override { return m_name; }
            static int thunk(lua_State* L);
            static void angelGeneric(asIScriptGeneric* gen);
            void bind(lua_State* L, Self* owner) override {
                if (!m_bindLua) return;
                lua_pushlightuserdata(L, owner);
                lua_pushlightuserdata(L, this);
                lua_pushcclosure(L, &Method::thunk, 2);
                lua_setfield(L, -2, m_name.c_str());
            }
            bool bindAngelScript(asIScriptEngine* engine, std::string_view className) override;
        };

        struct PropertyBase {
            virtual ~PropertyBase() = default;
            virtual std::string_view name() const = 0;
            virtual int get(lua_State* L, T* obj) = 0;
            virtual int set(lua_State* L, T* obj) = 0;
            virtual bool bindAngelScript(asIScriptEngine* engine, std::string_view className) = 0;
        };

        template<typename Field>
        struct Property : PropertyBase {
            std::string m_name;
            Field T::* m_member;
            explicit Property(std::string_view n, Field T::* m) : m_name(std::move(n)), m_member(m) {}
            std::string_view name() const override { return m_name; }
            int get(lua_State* L, T* obj) override { return Self::pushValue<Field>(L, obj->*m_member); }
            int set(lua_State* L, T* obj) override { obj->*m_member = Self::template readValue<Field>(L, 3); return 0; }
            static void angelGet(asIScriptGeneric* gen);
            static void angelSet(asIScriptGeneric* gen);
            bool bindAngelScript(asIScriptEngine* engine, std::string_view className) override;
        };

        struct FactoryBase {
            virtual ~FactoryBase() = default;
            virtual bool bindAngelScript(asIScriptEngine* engine, std::string_view className) = 0;
        };

        template<typename... Args>
        struct Factory final : FactoryBase {
            static void angelGeneric(asIScriptGeneric* gen);
            bool bindAngelScript(asIScriptEngine* engine, std::string_view className) override;
        };

        template<auto FixedArg, auto... FixedArgs>
        struct FixedFactory final : FactoryBase {
            static void angelGeneric(asIScriptGeneric* gen);
            bool bindAngelScript(asIScriptEngine* engine, std::string_view className) override;
        };

        template<typename Ret, typename... Args>
        struct FunctionFactory final : FactoryBase {
            Ret(*m_factory)(Args...) = nullptr;
            explicit FunctionFactory(Ret(*factory)(Args...)) : m_factory(factory) {}
            static void angelGeneric(asIScriptGeneric* gen);
            bool bindAngelScript(asIScriptEngine* engine, std::string_view className) override;
        };

        std::function<int(lua_State*)> m_ctor;
        std::unique_ptr<FactoryBase> m_angelFactory;
        std::vector<std::unique_ptr<MethodBase>> m_methods;
        std::vector<std::unique_ptr<PropertyBase>> m_properties;

        void pushToState(lua_State* L) override;
        bool registerAngelScriptType(asIScriptEngine* engine) override;
        bool pushToAngelScript(asIScriptEngine* engine) override;

        template<typename Field> static int  pushValue(lua_State* L, const Field& v);
        template<typename Field> static Field readValue(lua_State* L, int idx);
        template<typename Ret2, typename F, typename... Args2>
        static int callAndPush(lua_State* L, F&& f, int firstArgIdx);
    };

    static lua_State* getStateOrNull(Identifier scriptId, std::string_view action);
    static asIScriptEngine* getAngelEngineOrNull(Identifier scriptId, std::string_view action);
    static asIScriptModule* getAngelModuleOrNull(Identifier scriptId, std::string_view action);
    static bool ensureScriptThreadOrLog(Identifier scriptId, std::string_view action);
    [[noreturn]] static void failScriptAction(std::string_view message);

    // Primitives & class-pointer marshaling
    template<typename T> static void push(lua_State* L, const T& v);
    template<typename T> static T    check(lua_State* L, int idx);
    template<typename T> static void push(lua_State* L, const AdoptPtr<T>& v); // adopt overload

    template<typename T> static void pushClassPtr(lua_State* L, T* ptr, const std::string& metaName, bool owned);
    template<typename T> static void pushGcClassPtr(lua_State* L, std::shared_ptr<T> ptr, const std::string& metaName);
    template<typename T> static UserdataBox<T>* checkClassBox(lua_State* L, int idx, const std::string& metaName);
    template<typename T> static T* checkClassPtr(lua_State* L, int idx, const std::string& metaName);

    static void setGlobalVariant(lua_State* L, std::string_view name, const RegisteredVariable& v);
    static bool setAngelGlobal(asIScriptEngine* engine, std::string_view name, RegisteredVariable& v);
    static void exposeAngelProfile(asIScriptEngine* engine, std::string_view bindingProfile);
    static void exposeAngelProfiles(asIScriptEngine* engine, const std::vector<std::string>& bindingProfiles);
    static void exposeLuaProfile(lua_State* L, std::string_view bindingProfile);
    static void exposeLuaProfiles(lua_State* L, const std::vector<std::string>& bindingProfiles);
    static void ensureBindingProfileRegistered(std::string_view bindingProfile);
    static void ensureBindingProfilesRegistered(const std::vector<std::string>& bindingProfiles);
    static std::vector<std::string> bindingProfileList(std::string_view bindingProfile);
    static std::vector<std::string> bindingProfileList(std::initializer_list<std::string_view> bindingProfiles);
    static std::vector<std::string> bindingProfileList(const std::vector<std::string>& bindingProfiles);
    static std::vector<std::string> bindingProfileList(const std::vector<std::string_view>& bindingProfiles);
    static std::string bindingProfileKey(const std::vector<std::string>& bindingProfiles);
    static void rememberProfileSymbol(std::string_view name, std::vector<std::string>& orderedNames, std::unordered_set<std::string>& knownNames);

    template<typename T> static std::string angelTypeName();
    template<typename... Args> static std::string angelParameterList();
    template<typename T> static T angelGetGenericArg(asIScriptGeneric* gen, int idx);
    template<typename T> static T angelGetContextReturn(asIScriptContext* ctx);
    template<typename T> static void angelSetGenericReturn(asIScriptGeneric* gen, const T& value);
    template<typename T> static void angelSetContextArg(asIScriptContext* ctx, int idx, T&& value);
    template<typename T> static T* angelOwnedFactoryValue(const T& value);
    template<typename T> static void angelAddRef(T* ptr);
    template<typename T> static void angelRelease(T* ptr);

    struct CompiledChunk {
        std::vector<char> m_bytecode;
        std::string m_compileError;
        bool m_valid = false;
    };

    inline static Identifier sm_nextScriptID = 1;
    inline static std::unordered_map<Identifier, lua_State*> sm_states;
    inline static std::unordered_map<Identifier, asIScriptEngine*> sm_angelEngines;
    inline static std::unordered_map<Identifier, asIScriptModule*> sm_angelModules;
    inline static std::unordered_map<Identifier, int> sm_scriptRefCounts;
    inline static std::unordered_map<Identifier, std::string> sm_scriptPathById;
    inline static std::unordered_map<Identifier, ScriptLanguage> sm_scriptLanguageById;
    inline static std::unordered_map<Identifier, std::thread::id> sm_scriptThreadById;
    inline static std::unordered_map<std::string, Identifier> sm_sharedScriptsByPath;
    inline static std::unordered_map<std::string, CompiledChunk> sm_compiledChunkCache;

    inline static std::unordered_map<std::string, std::unique_ptr<RegisteredFunctionBase>> sm_functions;
    inline static std::unordered_map<std::string, RegisteredVariable> sm_variables;
    inline static std::unordered_map<std::string, std::unique_ptr<RegisteredClassBase>> sm_classes;

    inline static std::unordered_map<std::type_index, std::string> sm_typeToClassName;
    inline static bool sm_initialized = false;
    inline static std::string sm_defaultBindingProfile = "none";

    struct BindingProfile {
        void(*m_callback)() = nullptr;
        bool m_registered = false;
        std::vector<std::string> m_functions;
        std::vector<std::string> m_variables;
        std::vector<std::string> m_classes;
        std::unordered_set<std::string> m_functionNames;
        std::unordered_set<std::string> m_variableNames;
        std::unordered_set<std::string> m_classNames;
    };
    inline static std::unordered_map<std::string, BindingProfile> sm_bindingProfiles;
    inline static BindingProfile* sm_currentBindingProfile = nullptr;

    struct AngelOwnedObject {
        int m_refCount = 1;
        void(*m_deleter)(void*) = nullptr;
    };
    inline static std::unordered_map<void*, AngelOwnedObject> sm_angelOwnedObjects;
    inline static std::vector<std::shared_ptr<void>> sm_angelSharedKeepAlive;

    struct ClassBuildContextBase { virtual ~ClassBuildContextBase() = default; virtual std::string_view name() const = 0; };
    template<typename T> struct ClassBuildContext : ClassBuildContextBase {
        std::string m_name;
        explicit ClassBuildContext(std::string_view n) : m_name(std::move(n)) {}
        std::string_view name() const override { return m_name; }
    };
    inline static thread_local std::unique_ptr<ClassBuildContextBase> smt_buildingClass;
    inline static thread_local Identifier smt_activeScriptID = Identifier::INVALID;
};

// Hell is a separate file
#include "ScriptEngineTemplateHell.hpp"

