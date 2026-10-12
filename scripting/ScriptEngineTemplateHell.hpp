/*
 * File: ScriptEngineTemplateHell.h(ell)pp
 *
 * Description: Template implementations for ScriptEngine. I keep it out of the .cpp to avoid linker sadness.
 * Disclaimer: This File may cause unpleasant headache while reading. (I wrote this after WRITING this so I know what I'm talking about!)
 * But despite that it's pretty logical and does its job
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "../essentials/Logger.hpp"

template<typename T>
struct SEIsSharedPtr : std::false_type {};

template<typename T>
struct SEIsSharedPtr<std::shared_ptr<T>> : std::true_type {};

template<typename T>
struct SEIsAdoptPtr : std::false_type {};

template<typename T>
struct SEIsAdoptPtr<ScriptEngine::AdoptPtr<T>> : std::true_type {};

template<typename>
inline constexpr bool SEAlwaysFalse = false;

template<auto Fn>
struct SEAdoptReturnImpl;

template<typename Ret, typename... Args, Ret(*Fn)(Args...)>
struct SEAdoptReturnImpl<Fn> {
    static_assert(std::is_pointer_v<Ret>, "adoptReturn requires a pointer return type");
    static ScriptEngine::AdoptPtr<std::remove_pointer_t<Ret>> func(Args... a) {
        return { Fn(std::forward<Args>(a)...) };
    }
};

template<auto Fn>
constexpr auto ScriptEngine::adoptReturn() {
    return &SEAdoptReturnImpl<Fn>::func; // signature: AdoptPtr<T>(Args...)
}

// RegisteredFunction thunk
template<typename Ret, typename... Args>
int ScriptEngine::RegisteredFunction<Ret, Args...>::thunk(lua_State* L) {
    auto* self = static_cast<RegisteredFunction<Ret, Args...>*>(lua_touserdata(L, lua_upvalueindex(1)));
    if constexpr (std::is_void_v<Ret>) {
        auto call = [&]<std::size_t... I>(std::index_sequence<I...>) { self->m_func(ScriptEngine::check<Args>(L, int(1 + I))...); };
        call(std::make_index_sequence<sizeof...(Args)>{});
        return 0;
    } else {
        auto result = [&]<std::size_t... I>(std::index_sequence<I...>) { return self->m_func(ScriptEngine::check<Args>(L, int(1 + I))...); }(std::make_index_sequence<sizeof...(Args)>{});
        ScriptEngine::push<Ret>(L, result);
        return 1;
    }
}

template<typename Ret, typename... Args>
void ScriptEngine::RegisteredFunction<Ret, Args...>::angelGeneric(asIScriptGeneric* gen) {
    auto* self = static_cast<RegisteredFunction<Ret, Args...>*>(gen->GetAuxiliary());
    if constexpr (std::is_void_v<Ret>) {
        auto call = [&]<std::size_t... I>(std::index_sequence<I...>) {
            self->m_func(ScriptEngine::angelGetGenericArg<Args>(gen, int(I))...);
        };
        call(std::make_index_sequence<sizeof...(Args)>{});
    } else {
        auto result = [&]<std::size_t... I>(std::index_sequence<I...>) {
            return self->m_func(ScriptEngine::angelGetGenericArg<Args>(gen, int(I))...);
        }(std::make_index_sequence<sizeof...(Args)>{});
        ScriptEngine::angelSetGenericReturn<Ret>(gen, result);
    }
}

template<typename Ret, typename... Args>
bool ScriptEngine::RegisteredFunction<Ret, Args...>::pushToAngelScript(asIScriptEngine* engine, std::string_view name) {
    const std::string decl = ScriptEngine::angelTypeName<Ret>() + " " + std::string(name) + "(" + ScriptEngine::angelParameterList<Args...>() + ")";
    const int rc = engine->RegisterGlobalFunction(decl.c_str(), asFUNCTION((RegisteredFunction<Ret, Args...>::angelGeneric)), asCALL_GENERIC, this);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript function: " + decl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

// Registration templates
template<typename T>
void ScriptEngine::addGlobalVariable(std::string_view name, const T& value) {
    RegisteredVariable v;
    v.m_kind = RegisteredVariable::Kind::Value;
    v.m_type = std::type_index(typeid(T));
    if constexpr (std::is_same_v<T, std::string>) { v.m_value = value; } else if constexpr (std::is_same_v<T, const char*>) { v.m_value = std::string(value); } else if constexpr (std::is_same_v<T, bool>) { v.m_value = value; } else if constexpr (std::is_integral_v<T>) { v.m_value = static_cast<int64_t>(value); } else if constexpr (std::is_floating_point_v<T>) { v.m_value = static_cast<double>(value); } else { static_assert(!sizeof(T), "Unsupported variable type for addGlobalVariable"); }
    sm_variables[std::string(name)] = std::move(v);
    if (sm_currentBindingProfile != nullptr) rememberProfileSymbol(name, sm_currentBindingProfile->m_variables, sm_currentBindingProfile->m_variableNames);
}

template<typename T>
void ScriptEngine::addGlobalVariableRef(std::string_view name, T* ptr) {
    RegisteredVariable v;
    v.m_kind = RegisteredVariable::Kind::Ref;
    v.m_ref = ptr;
    v.m_type = std::type_index(typeid(T));
    sm_variables[std::string(name)] = std::move(v);
    if (sm_currentBindingProfile != nullptr) rememberProfileSymbol(name, sm_currentBindingProfile->m_variables, sm_currentBindingProfile->m_variableNames);
}

template<typename Ret, typename... Args>
void ScriptEngine::addGlobalFunction(std::string_view name, Ret(*func)(Args...)) {
    sm_functions[std::string(name)] = std::make_unique<RegisteredFunction<Ret, Args...>>(std::function<Ret(Args...)>(func));
    if (sm_currentBindingProfile != nullptr) rememberProfileSymbol(name, sm_currentBindingProfile->m_functions, sm_currentBindingProfile->m_functionNames);
}

template<typename T>
void ScriptEngine::beginClass(std::string_view name) {
    auto rc = std::make_unique<RegisteredClass<T>>();
    rc->m_name = std::string(name);
    rc->m_metaName = std::string(name) + "__meta";
    sm_typeToClassName[std::type_index(typeid(T))] = std::string(name);
    sm_classes[std::string(name)] = std::move(rc);
    if (sm_currentBindingProfile != nullptr) rememberProfileSymbol(name, sm_currentBindingProfile->m_classes, sm_currentBindingProfile->m_classNames);
    smt_buildingClass = std::make_unique<ClassBuildContext<T>>(name);
}

template<typename T, typename... CtorArgs>
void ScriptEngine::addConstructor() {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstructor<T> without matching beginClass<T>");
    auto* base = sm_classes.at(std::string(ctx->name())).get();
    auto* rc = dynamic_cast<RegisteredClass<T>*>(base);
    rc->m_ctor = [](lua_State* L) -> int {
        auto make = [&]<std::size_t... I>(std::index_sequence<I...>) {
            return std::make_shared<T>(ScriptEngine::check<CtorArgs>(L, int(1 + I))...);
        };
        ScriptEngine::push<std::shared_ptr<T>>(L, make(std::make_index_sequence<sizeof...(CtorArgs)>{}));
        return 1;
    };
    rc->m_angelFactory = std::make_unique<typename RegisteredClass<T>::template Factory<CtorArgs...>>();
}

template<typename T, auto FixedArg, auto... FixedArgs>
void ScriptEngine::addConstructor() {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstructor<T, fixed args...> without matching beginClass<T>");
    auto* base = sm_classes.at(std::string(ctx->name())).get();
    auto* rc = dynamic_cast<RegisteredClass<T>*>(base);
    rc->m_ctor = [](lua_State* L) -> int {
        ScriptEngine::push<std::shared_ptr<T>>(L, std::make_shared<T>(FixedArg, FixedArgs...));
        return 1;
    };
    rc->m_angelFactory = std::make_unique<typename RegisteredClass<T>::template FixedFactory<FixedArg, FixedArgs...>>();
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addConstructor(Ret(*factory)(Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstructor(factory) without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_ctor = [factory](lua_State* L) -> int {
        static_assert(!std::is_void_v<Ret>, "Factory constructors must return a value");
        auto make = [&]<std::size_t... I>(std::index_sequence<I...>) {
            return factory(ScriptEngine::check<Args>(L, int(1 + I))...);
        };
        ScriptEngine::push<Ret>(L, make(std::make_index_sequence<sizeof...(Args)>{}));
        return 1;
    };
    rc->m_angelFactory = std::make_unique<typename RegisteredClass<T>::template FunctionFactory<Ret, Args...>>(factory);
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addMethod(std::string_view name, Ret(T::* method)(Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addMethod<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [method](T* self, Args... a) { return (self->*method)(a...); }));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addMethodLua(std::string_view name, Ret(T::* method)(Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addMethodLua<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [method](T* self, Args... a) { return (self->*method)(a...); }, true, false));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addMethodAngelScript(std::string_view name, Ret(T::* method)(Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addMethodAngelScript<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [method](T* self, Args... a) { return (self->*method)(a...); }, false, true));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addMethod(std::string_view name, Ret(*func)(T*, Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addMethod<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [func](T* self, Args... a) { return func(self, a...); }));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addMethodLua(std::string_view name, Ret(*func)(T*, Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addMethodLua<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [func](T* self, Args... a) { return func(self, a...); }, true, false));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addMethodAngelScript(std::string_view name, Ret(*func)(T*, Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addMethodAngelScript<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [func](T* self, Args... a) { return func(self, a...); }, false, true));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addConstMethod(std::string_view name, Ret(T::* method)(Args...) const) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstMethod<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [method](T* self, Args... a) { return (self->*method)(a...); }));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addConstMethodLua(std::string_view name, Ret(T::* method)(Args...) const) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstMethodLua<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [method](T* self, Args... a) { return (self->*method)(a...); }, true, false));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addConstMethodAngelScript(std::string_view name, Ret(T::* method)(Args...) const) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstMethodAngelScript<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [method](T* self, Args... a) { return (self->*method)(a...); }, false, true));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addConstMethod(std::string_view name, Ret(*func)(const T*, Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstMethod<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [func](T* self, Args... a) { return func(self, a...); }));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addConstMethodLua(std::string_view name, Ret(*func)(const T*, Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstMethodLua<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [func](T* self, Args... a) { return func(self, a...); }, true, false));
}

template<typename T, typename Ret, typename... Args>
void ScriptEngine::addConstMethodAngelScript(std::string_view name, Ret(*func)(const T*, Args...)) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addConstMethodAngelScript<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_methods.emplace_back(std::make_unique<typename RegisteredClass<T>::template Method<Ret, Args...>>(
        name, [func](T* self, Args... a) { return func(self, a...); }, false, true));
}

template<typename T, typename Field>
void ScriptEngine::addProperty(std::string_view name, Field T::* field) {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("addProperty<T> without matching beginClass<T>");
    auto* rc = dynamic_cast<RegisteredClass<T>*>(sm_classes.at(std::string(ctx->name())).get());
    rc->m_properties.emplace_back(std::make_unique<typename RegisteredClass<T>::template Property<Field>>(name, field));
}

template<typename T>
void ScriptEngine::endClass() {
    auto* ctx = dynamic_cast<ClassBuildContext<T>*>(smt_buildingClass.get());
    if (!ctx) throw std::runtime_error("endClass<T> without matching beginClass<T>");
    smt_buildingClass.reset();
}

// push/check for primitives
template<> inline void ScriptEngine::push<bool>(lua_State* L, const bool& v) { lua_pushboolean(L, v ? 1 : 0); }
template<> inline void ScriptEngine::push<int>(lua_State* L, const int& v) { lua_pushinteger(L, static_cast<lua_Integer>(v)); }
template<> inline void ScriptEngine::push<int64_t>(lua_State* L, const int64_t& v) { lua_pushinteger(L, static_cast<lua_Integer>(v)); }
template<> inline void ScriptEngine::push<double>(lua_State* L, const double& v) { lua_pushnumber(L, static_cast<lua_Number>(v)); }
template<> inline void ScriptEngine::push<std::string>(lua_State* L, const std::string& v) { lua_pushlstring(L, v.data(), v.size()); }
template<> inline void ScriptEngine::push<std::string_view>(lua_State* L, const std::string_view& v) { lua_pushlstring(L, v.data(), v.size()); }
template<> inline void ScriptEngine::push<const char*>(lua_State* L, const char* const& v) { lua_pushstring(L, v); }
template<> inline void ScriptEngine::push<float>(lua_State* L, const float& v) { lua_pushnumber(L, static_cast<lua_Number>(v)); }

template<> inline bool        ScriptEngine::check<bool>(lua_State* L, int idx) { return lua_toboolean(L, idx) != 0; }
template<> inline int         ScriptEngine::check<int>(lua_State* L, int idx) { return static_cast<int>(luaL_checkinteger(L, idx)); }
template<> inline int64_t     ScriptEngine::check<int64_t>(lua_State* L, int idx) { return static_cast<int64_t>(luaL_checkinteger(L, idx)); }
template<> inline double      ScriptEngine::check<double>(lua_State* L, int idx) { return static_cast<double>(luaL_checknumber(L, idx)); }
template<> inline std::string ScriptEngine::check<std::string>(lua_State* L, int idx) { size_t n = 0; const char* s = luaL_checklstring(L, idx, &n); return std::string(s, n); }
template<> inline std::string_view ScriptEngine::check<std::string_view>(lua_State* L, int idx) { size_t n = 0; const char* s = luaL_checklstring(L, idx, &n); return std::string_view(s, n); }
template<> inline float ScriptEngine::check<float>(lua_State* L, int idx) { return static_cast<float>(luaL_checknumber(L, idx)); }

// generic pointer marshaling (non-adopt → owned=false)
template<typename T>
void ScriptEngine::push(lua_State* L, const T& v) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;

    if constexpr (SEIsSharedPtr<U>::value) {
        using P = std::remove_cv_t<typename U::element_type>;
        auto it = sm_typeToClassName.find(std::type_index(typeid(P)));
        if (it == sm_typeToClassName.end()) throw std::runtime_error("push<T*>: class not registered");
        const std::string& className = it->second;
        const std::string& metaName = sm_classes.at(className)->m_metaName;
        pushGcClassPtr<P>(L, v, metaName);
    } else if constexpr (std::is_pointer_v<U>) {
        using P = std::remove_cv_t<std::remove_pointer_t<U>>;
        auto it = sm_typeToClassName.find(std::type_index(typeid(P)));
        if (it == sm_typeToClassName.end()) throw std::runtime_error("push<T*>: class not registered");
        const std::string& className = it->second;
        const std::string& metaName = sm_classes.at(className)->m_metaName;
        pushClassPtr<P>(L, const_cast<P*>(v), metaName, /*owned*/false);
    } else if constexpr (std::is_class_v<U>) {
        auto it = sm_typeToClassName.find(std::type_index(typeid(U)));
        if (it == sm_typeToClassName.end()) {
            throw std::runtime_error("push<T>: class not registered yet: " + std::string(typeid(U).name())); // Runtime hack
        }
        const std::string& className = it->second;
        const std::string& metaName = sm_classes.at(className)->m_metaName;
        pushGcClassPtr<U>(L, std::make_shared<U>(v), metaName);
    } else {
        static_assert(!sizeof(T), "Unsupported type for push");
    }
}


// adopt overload (owned=true)
template<typename T>
void ScriptEngine::push(lua_State* L, const AdoptPtr<T>& v) {
    auto it = sm_typeToClassName.find(std::type_index(typeid(T)));
    if (it == sm_typeToClassName.end()) throw std::runtime_error("push<AdoptPtr<T>>: class not registered");
    const std::string& className = it->second;
    const std::string& metaName = sm_classes.at(className)->m_metaName;
    pushClassPtr<T>(L, v.ptr, metaName, true);
}

template<typename T>
T ScriptEngine::check(lua_State* L, int idx) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;

    if constexpr (SEIsSharedPtr<U>::value) {
        using P = std::remove_cv_t<typename U::element_type>;
        auto it = sm_typeToClassName.find(std::type_index(typeid(P)));
        if (it == sm_typeToClassName.end())
            throw std::runtime_error("check<shared_ptr<T>>: class not registered");
        const std::string& className = it->second;
        const std::string& metaName = sm_classes.at(className)->m_metaName;
        auto* box = checkClassBox<P>(L, idx, metaName);
        if (!box->gcPtr) luaL_error(L, "object is not Lua GC-managed");
        return box->gcPtr;
    } else if constexpr (std::is_pointer_v<U>) {
        using P = std::remove_cv_t<std::remove_pointer_t<U>>;
        auto it = sm_typeToClassName.find(std::type_index(typeid(P)));
        if (it == sm_typeToClassName.end())
            throw std::runtime_error("check<T*>: class not registered");
        const std::string& className = it->second;
        const std::string& metaName = sm_classes.at(className)->m_metaName;
        return checkClassPtr<P>(L, idx, metaName);
    } else if constexpr (std::is_class_v<U>) {
        auto it = sm_typeToClassName.find(std::type_index(typeid(U)));
        if (it == sm_typeToClassName.end())
            throw std::runtime_error("check<T>: class not registered");
        const std::string& className = it->second;
        const std::string& metaName = sm_classes.at(className)->m_metaName;
        U* p = checkClassPtr<U>(L, idx, metaName);
        if constexpr (std::is_reference_v<T>)
            return *p;
        else
            return *p;
    } else {
        static_assert(!sizeof(T), "Unsupported type for check");
    }
}

template<typename T>
void ScriptEngine::pushClassPtr(lua_State* L, T* ptr, const std::string& metaName, bool owned) {
    auto* box = static_cast<UserdataBox<T>*>(lua_newuserdata(L, sizeof(UserdataBox<T>)));
    std::construct_at(box);
    box->ptr = ptr;
    box->owned = owned;
    luaL_getmetatable(L, metaName.c_str());
    lua_setmetatable(L, -2);
    lua_newtable(L);
    lua_setiuservalue(L, -2, 1);
}

template<typename T>
void ScriptEngine::pushGcClassPtr(lua_State* L, std::shared_ptr<T> ptr, const std::string& metaName) {
    auto* box = static_cast<UserdataBox<T>*>(lua_newuserdata(L, sizeof(UserdataBox<T>)));
    std::construct_at(box);
    box->gcPtr = std::move(ptr);
    box->ptr = box->gcPtr.get();
    luaL_getmetatable(L, metaName.c_str());
    lua_setmetatable(L, -2);
    lua_newtable(L);
    lua_setiuservalue(L, -2, 1);
}

template<typename T>
typename ScriptEngine::template UserdataBox<T>* ScriptEngine::checkClassBox(lua_State* L, int idx, const std::string& metaName) {
    auto* box = static_cast<UserdataBox<T>*>(luaL_checkudata(L, idx, metaName.c_str()));
    if (!box || !box->ptr) luaL_error(L, "null object");
    return box;
}

template<typename T>
T* ScriptEngine::checkClassPtr(lua_State* L, int idx, const std::string& metaName) {
    auto* box = checkClassBox<T>(L, idx, metaName);
    return box->ptr;
}

// RegisteredClass methods
template<typename T>
void ScriptEngine::RegisteredClass<T>::pushToState(lua_State* L) {
    luaL_newmetatable(L, m_metaName.c_str());

    lua_getfield(L, -1, "__gc");
    bool hasGc = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!hasGc) {
        lua_pushstring(L, m_metaName.c_str());
        lua_pushcclosure(L, [](lua_State* L) -> int {
            (void)lua_tostring(L, lua_upvalueindex(1));
            auto* box = static_cast<UserdataBox<T>*>(lua_touserdata(L, 1));
            if (box) {
                if constexpr (std::is_destructible_v<T>) {
                    if (box->owned && box->ptr) delete box->ptr;
                }
                box->ptr = nullptr;
                box->gcPtr.reset();
                std::destroy_at(box);
            }
            return 0;
            }, 1);
        lua_setfield(L, -2, "__gc");
    }

    lua_getfield(L, -1, "__methods");
    bool haveMethods = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!haveMethods) {
        lua_newtable(L);
        for (auto& m : m_methods) m->bind(L, this);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "__methods");
        lua_pop(L, 1);
    }

    lua_getfield(L, -1, "__index");
    bool haveIndex = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!haveIndex) {
        lua_pushlightuserdata(L, this);
        lua_getfield(L, -2, "__methods");
        lua_pushcclosure(L, [](lua_State* L)->int {
            auto* owner = static_cast<RegisteredClass<T>*>(lua_touserdata(L, lua_upvalueindex(1)));
            lua_pushvalue(L, lua_upvalueindex(2));
            lua_pushvalue(L, 2);
            lua_gettable(L, -2);
            if (!lua_isnil(L, -1)) {
                lua_pushvalue(L, 1);
                lua_pushcclosure(L, [](lua_State* L) -> int {
                    const int nargs = lua_gettop(L);
                    bool hasExplicitSelf = false;
                    if (nargs > 0) {
                        lua_pushvalue(L, 1);
                        lua_pushvalue(L, lua_upvalueindex(2));
                        hasExplicitSelf = lua_rawequal(L, -1, -2) != 0;
                        lua_pop(L, 2);
                    }

                    if (hasExplicitSelf) {
                        lua_remove(L, 1);
                    }

                    lua_pushvalue(L, lua_upvalueindex(1));
                    lua_insert(L, 1);
                    lua_pushvalue(L, lua_upvalueindex(2));
                    lua_insert(L, 2);
                    lua_call(L, hasExplicitSelf ? nargs : nargs + 1, LUA_MULTRET);
                    return lua_gettop(L);
                }, 2);
                return 1;
            }
            lua_pop(L, 2);
            size_t klen = 0; const char* key = luaL_checklstring(L, 2, &klen);
            auto* box = static_cast<UserdataBox<T>*>(luaL_checkudata(L, 1, owner->m_metaName.c_str()));
            T* obj = box->ptr;
            for (auto& p : owner->m_properties) {
                if (p->name() == std::string(key, klen)) return p->get(L, obj);
            }
            lua_getiuservalue(L, 1, 1);
            lua_pushvalue(L, 2);
            lua_gettable(L, -2);
            return 1;
            }, 2);
        lua_setfield(L, -2, "__index");
    }

    lua_getfield(L, -1, "__newindex");
    bool haveNewIndex = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!haveNewIndex) {
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, [](lua_State* L)->int {
            auto* owner = static_cast<RegisteredClass<T>*>(lua_touserdata(L, lua_upvalueindex(1)));
            size_t klen = 0; const char* key = luaL_checklstring(L, 2, &klen);
            auto* box = static_cast<UserdataBox<T>*>(luaL_checkudata(L, 1, owner->m_metaName.c_str()));
            T* obj = box->ptr;
            for (auto& p : owner->m_properties) {
                if (p->name() == std::string(key, klen)) return p->set(L, obj);
            }
            lua_getiuservalue(L, 1, 1);
            lua_pushvalue(L, 2);
            lua_pushvalue(L, 3);
            lua_settable(L, -3);
            return 0;
            }, 1);
        lua_setfield(L, -2, "__newindex");
    }

    lua_pop(L, 1);

    lua_newtable(L);
    auto* self = this;
    lua_pushlightuserdata(L, self);
    lua_pushcclosure(L, [](lua_State* L)->int {
        auto* self = static_cast<RegisteredClass<T>*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (self->m_ctor) return self->m_ctor(L);
        if constexpr (std::is_default_constructible_v<T>) {
            ScriptEngine::push<std::shared_ptr<T>>(L, std::make_shared<T>());
            return 1;
        } else {
            return luaL_error(L, "No constructor registered for class '%s'", self->m_name.c_str());
        }
        }, 1);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, m_name.c_str());
}

template<typename T>
bool ScriptEngine::RegisteredClass<T>::registerAngelScriptType(asIScriptEngine* engine) {
    if (engine == nullptr) return false;
    if (engine->GetTypeInfoByName(m_name.c_str()) != nullptr) return true;

    int rc = engine->RegisterObjectType(m_name.c_str(), 0, asOBJ_REF);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript class: " + m_name + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

template<typename T>
bool ScriptEngine::RegisteredClass<T>::pushToAngelScript(asIScriptEngine* engine) {
    if (!registerAngelScriptType(engine)) return false;

    engine->RegisterObjectBehaviour(m_name.c_str(), asBEHAVE_ADDREF, "void f()", asFUNCTION(ScriptEngine::angelAddRef<T>), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour(m_name.c_str(), asBEHAVE_RELEASE, "void f()", asFUNCTION(ScriptEngine::angelRelease<T>), asCALL_CDECL_OBJLAST);

    bool ok = true;
    if (m_angelFactory) ok = m_angelFactory->bindAngelScript(engine, m_name) && ok;
    else if constexpr (std::is_default_constructible_v<T>) {
        Factory<> fallback;
        ok = fallback.bindAngelScript(engine, m_name) && ok;
    }

    for (auto& m : m_methods) ok = m->bindAngelScript(engine, m_name) && ok;
    for (auto& p : m_properties) ok = p->bindAngelScript(engine, m_name) && ok;
    return ok;
}

template<typename T>
template<typename Ret, typename... Args>
int ScriptEngine::RegisteredClass<T>::Method<Ret, Args...>::thunk(lua_State* L) {
    auto* owner = static_cast<Self*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* self = static_cast<Method<Ret, Args...>*>(lua_touserdata(L, lua_upvalueindex(2)));
    auto* box = static_cast<UserdataBox<T>*>(luaL_checkudata(L, 1, owner->m_metaName.c_str()));
    T* obj = box->ptr;
    auto bound = [obj, self](Args... a) -> Ret { return self->m_fn(obj, a...); };
    using Bound = decltype(bound);
    return Self::template callAndPush<Ret, Bound, Args...>(L, std::move(bound), 2);
}

template<typename T>
template<typename Ret, typename... Args>
void ScriptEngine::RegisteredClass<T>::Method<Ret, Args...>::angelGeneric(asIScriptGeneric* gen) {
    auto* self = static_cast<Method<Ret, Args...>*>(gen->GetAuxiliary());
    auto* obj = static_cast<T*>(gen->GetObject());
    if constexpr (std::is_void_v<Ret>) {
        auto call = [&]<std::size_t... I>(std::index_sequence<I...>) {
            self->m_fn(obj, ScriptEngine::angelGetGenericArg<Args>(gen, int(I))...);
        };
        call(std::make_index_sequence<sizeof...(Args)>{});
    } else {
        auto result = [&]<std::size_t... I>(std::index_sequence<I...>) {
            return self->m_fn(obj, ScriptEngine::angelGetGenericArg<Args>(gen, int(I))...);
        }(std::make_index_sequence<sizeof...(Args)>{});
        ScriptEngine::angelSetGenericReturn<Ret>(gen, result);
    }
}

template<typename T>
template<typename Ret, typename... Args>
bool ScriptEngine::RegisteredClass<T>::Method<Ret, Args...>::bindAngelScript(asIScriptEngine* engine, std::string_view className) {
    if (!m_bindAngelScript) {
        return true;
    }
    const std::string decl = ScriptEngine::angelTypeName<Ret>() + " " + m_name + "(" + ScriptEngine::angelParameterList<Args...>() + ")";
    const int rc = engine->RegisterObjectMethod(std::string(className).c_str(), decl.c_str(), asFUNCTION((Method<Ret, Args...>::angelGeneric)), asCALL_GENERIC, this);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript method: " + std::string(className) + "::" + decl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

template<typename T>
template<typename Field>
void ScriptEngine::RegisteredClass<T>::Property<Field>::angelGet(asIScriptGeneric* gen) {
    auto* self = static_cast<Property<Field>*>(gen->GetAuxiliary());
    auto* obj = static_cast<T*>(gen->GetObject());
    ScriptEngine::angelSetGenericReturn<Field>(gen, obj->*self->m_member);
}

template<typename T>
template<typename Field>
void ScriptEngine::RegisteredClass<T>::Property<Field>::angelSet(asIScriptGeneric* gen) {
    auto* self = static_cast<Property<Field>*>(gen->GetAuxiliary());
    auto* obj = static_cast<T*>(gen->GetObject());
    obj->*self->m_member = ScriptEngine::angelGetGenericArg<Field>(gen, 0);
}

template<typename T>
template<typename Field>
bool ScriptEngine::RegisteredClass<T>::Property<Field>::bindAngelScript(asIScriptEngine* engine, std::string_view className) {
    const std::string fieldType = ScriptEngine::angelTypeName<Field>();
    const std::string getterDecl = fieldType + " get_" + m_name + "() property";
    const std::string setterDecl = "void set_" + m_name + "(" + fieldType + " value) property";
    const std::string classNameString(className);
    int rc = engine->RegisterObjectMethod(classNameString.c_str(), getterDecl.c_str(), asFUNCTION(Property<Field>::angelGet), asCALL_GENERIC, this);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript property getter: " + classNameString + "::" + getterDecl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    rc = engine->RegisterObjectMethod(classNameString.c_str(), setterDecl.c_str(), asFUNCTION(Property<Field>::angelSet), asCALL_GENERIC, this);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript property setter: " + classNameString + "::" + setterDecl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

template<typename T>
template<typename... Args>
void ScriptEngine::RegisteredClass<T>::Factory<Args...>::angelGeneric(asIScriptGeneric* gen) {
    auto make = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return new T(ScriptEngine::angelGetGenericArg<Args>(gen, int(I))...);
    };
    T* ptr = make(std::make_index_sequence<sizeof...(Args)>{});
    sm_angelOwnedObjects[ptr] = { 1, [](void* p) { delete static_cast<T*>(p); } };
    gen->SetReturnObject(ptr);
}

template<typename T>
template<typename... Args>
bool ScriptEngine::RegisteredClass<T>::Factory<Args...>::bindAngelScript(asIScriptEngine* engine, std::string_view className) {
    const std::string classNameString(className);
    const std::string decl = classNameString + "@ f(" + ScriptEngine::angelParameterList<Args...>() + ")";
    const int rc = engine->RegisterObjectBehaviour(classNameString.c_str(), asBEHAVE_FACTORY, decl.c_str(), asFUNCTION(Factory<Args...>::angelGeneric), asCALL_GENERIC);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript factory: " + classNameString + "::" + decl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

template<typename T>
template<auto FixedArg, auto... FixedArgs>
void ScriptEngine::RegisteredClass<T>::FixedFactory<FixedArg, FixedArgs...>::angelGeneric(asIScriptGeneric* gen) {
    T* ptr = new T(FixedArg, FixedArgs...);
    sm_angelOwnedObjects[ptr] = { 1, [](void* p) { delete static_cast<T*>(p); } };
    gen->SetReturnObject(ptr);
}

template<typename T>
template<auto FixedArg, auto... FixedArgs>
bool ScriptEngine::RegisteredClass<T>::FixedFactory<FixedArg, FixedArgs...>::bindAngelScript(asIScriptEngine* engine, std::string_view className) {
    const std::string classNameString(className);
    const std::string decl = classNameString + "@ f()";
    const int rc = engine->RegisterObjectBehaviour(classNameString.c_str(), asBEHAVE_FACTORY, decl.c_str(), asFUNCTION((FixedFactory<FixedArg, FixedArgs...>::angelGeneric)), asCALL_GENERIC);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript fixed factory: " + classNameString + "::" + decl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

template<typename T>
template<typename Ret, typename... Args>
void ScriptEngine::RegisteredClass<T>::FunctionFactory<Ret, Args...>::angelGeneric(asIScriptGeneric* gen) {
    auto* self = static_cast<FunctionFactory<Ret, Args...>*>(gen->GetAuxiliary());
    auto make = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return self->m_factory(ScriptEngine::angelGetGenericArg<Args>(gen, int(I))...);
    };
    auto result = make(std::make_index_sequence<sizeof...(Args)>{});
    ScriptEngine::angelSetGenericReturn<Ret>(gen, result);
}

template<typename T>
template<typename Ret, typename... Args>
bool ScriptEngine::RegisteredClass<T>::FunctionFactory<Ret, Args...>::bindAngelScript(asIScriptEngine* engine, std::string_view className) {
    const std::string classNameString(className);
    const std::string decl = classNameString + "@ f(" + ScriptEngine::angelParameterList<Args...>() + ")";
    const int rc = engine->RegisterObjectBehaviour(classNameString.c_str(), asBEHAVE_FACTORY, decl.c_str(), asFUNCTION((FunctionFactory<Ret, Args...>::angelGeneric)), asCALL_GENERIC, this);
    if (rc < 0 && rc != asALREADY_REGISTERED) {
        Logger::log(LET_ERROR, "Orka, Script Engine", "Failed to register AngelScript function factory: " + classNameString + "::" + decl + " (rc=" + std::to_string(rc) + ")");
        return false;
    }
    return true;
}

template<typename T>
template<typename Field>
int ScriptEngine::RegisteredClass<T>::pushValue(lua_State* L, const Field& v) {
    ScriptEngine::push<Field>(L, v); return 1;
}

template<typename T>
template<typename Field>
Field ScriptEngine::RegisteredClass<T>::readValue(lua_State* L, int idx) {
    return ScriptEngine::check<Field>(L, idx);
}

template<typename T>
template<typename Ret2, typename F, typename... Args2>
int ScriptEngine::RegisteredClass<T>::callAndPush(lua_State* L, F&& f, int firstArgIdx) {
    if constexpr (std::is_void_v<Ret2>) {
        auto call = [&]<std::size_t... I>(std::index_sequence<I...>) {
            std::forward<F>(f)(ScriptEngine::check<Args2>(L, firstArgIdx + int(I))...);
        };
        call(std::make_index_sequence<sizeof...(Args2)>{});
        return 0;
    } else {
        auto result = [&]<std::size_t... I>(std::index_sequence<I...>) {
            return std::forward<F>(f)(ScriptEngine::check<Args2>(L, firstArgIdx + int(I))...);
        }(std::make_index_sequence<sizeof...(Args2)>{});
        ScriptEngine::push<Ret2>(L, result);
        return 1;
    }
}

template<typename T>
std::string ScriptEngine::angelTypeName() {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_void_v<U>) return "void";
    else if constexpr (std::is_same_v<U, bool>) return "bool";
    else if constexpr (std::is_same_v<U, int>) return "int";
    else if constexpr (std::is_same_v<U, int64_t>) return "int64";
    else if constexpr (std::is_same_v<U, float>) return "float";
    else if constexpr (std::is_same_v<U, double>) return "double";
    else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view> || std::is_same_v<U, const char*>) return "string";
    else if constexpr (SEIsAdoptPtr<U>::value) {
        using P = std::remove_cv_t<decltype(*std::declval<U>().ptr)>;
        auto it = sm_typeToClassName.find(std::type_index(typeid(P)));
        if (it == sm_typeToClassName.end()) throw std::runtime_error("angelTypeName<AdoptPtr<T>>: class not registered");
        return it->second + "@";
    } else if constexpr (SEIsSharedPtr<U>::value) {
        using P = std::remove_cv_t<typename U::element_type>;
        auto it = sm_typeToClassName.find(std::type_index(typeid(P)));
        if (it == sm_typeToClassName.end()) throw std::runtime_error("angelTypeName<shared_ptr<T>>: class not registered");
        return it->second + "@";
    } else if constexpr (std::is_pointer_v<U>) {
        using P = std::remove_cv_t<std::remove_pointer_t<U>>;
        auto it = sm_typeToClassName.find(std::type_index(typeid(P)));
        if (it == sm_typeToClassName.end()) throw std::runtime_error("angelTypeName<T*>: class not registered");
        return it->second + "@";
    } else if constexpr (std::is_class_v<U>) {
        auto it = sm_typeToClassName.find(std::type_index(typeid(U)));
        if (it == sm_typeToClassName.end()) throw std::runtime_error("angelTypeName<T>: class not registered");
        return it->second + "@";
    } else {
        static_assert(SEAlwaysFalse<T>, "Unsupported AngelScript type");
    }
}

template<typename... Args>
std::string ScriptEngine::angelParameterList() {
    std::string out;
    bool first = true;
    auto append = [&](const std::string& type) {
        if (!first) out += ", ";
        out += type;
        first = false;
    };
    (append(angelTypeName<Args>()), ...);
    return out;
}

template<typename T>
T ScriptEngine::angelGetGenericArg(asIScriptGeneric* gen, int idx) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, bool>) return gen->GetArgByte(idx) != 0;
    else if constexpr (std::is_same_v<U, int>) return static_cast<int>(gen->GetArgDWord(idx));
    else if constexpr (std::is_same_v<U, int64_t>) return static_cast<int64_t>(gen->GetArgQWord(idx));
    else if constexpr (std::is_same_v<U, float>) return gen->GetArgFloat(idx);
    else if constexpr (std::is_same_v<U, double>) return gen->GetArgDouble(idx);
    else if constexpr (std::is_same_v<U, std::string>) return *static_cast<std::string*>(gen->GetArgObject(idx));
    else if constexpr (std::is_same_v<U, std::string_view>) return *static_cast<std::string*>(gen->GetArgObject(idx));
    else if constexpr (std::is_pointer_v<U>) return static_cast<U>(gen->GetArgObject(idx));
    else if constexpr (std::is_class_v<U> && !SEIsSharedPtr<U>::value) {
        auto* obj = static_cast<U*>(gen->GetArgObject(idx));
        if constexpr (std::is_reference_v<T>) return *obj;
        else return *obj;
    } else {
        static_assert(SEAlwaysFalse<T>, "Unsupported AngelScript argument type");
    }
}

template<typename T>
T ScriptEngine::angelGetContextReturn(asIScriptContext* ctx) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, bool>) return ctx->GetReturnByte() != 0;
    else if constexpr (std::is_same_v<U, int>) return static_cast<int>(ctx->GetReturnDWord());
    else if constexpr (std::is_same_v<U, int64_t>) return static_cast<int64_t>(ctx->GetReturnQWord());
    else if constexpr (std::is_same_v<U, float>) return ctx->GetReturnFloat();
    else if constexpr (std::is_same_v<U, double>) return ctx->GetReturnDouble();
    else if constexpr (std::is_same_v<U, std::string>) return *static_cast<std::string*>(ctx->GetReturnObject());
    else if constexpr (std::is_same_v<U, std::string_view>) return *static_cast<std::string*>(ctx->GetReturnObject());
    else if constexpr (std::is_pointer_v<U>) return static_cast<U>(ctx->GetReturnObject());
    else if constexpr (std::is_class_v<U> && !SEIsSharedPtr<U>::value) {
        auto* obj = static_cast<U*>(ctx->GetReturnObject());
        if constexpr (std::is_reference_v<T>) return *obj;
        else return *obj;
    } else {
        static_assert(SEAlwaysFalse<T>, "Unsupported AngelScript return type");
    }
}

template<typename T>
void ScriptEngine::angelSetGenericReturn(asIScriptGeneric* gen, const T& value) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, bool>) gen->SetReturnByte(value ? 1 : 0);
    else if constexpr (std::is_same_v<U, int>) gen->SetReturnDWord(static_cast<asDWORD>(value));
    else if constexpr (std::is_same_v<U, int64_t>) gen->SetReturnQWord(static_cast<asQWORD>(value));
    else if constexpr (std::is_same_v<U, float>) gen->SetReturnFloat(value);
    else if constexpr (std::is_same_v<U, double>) gen->SetReturnDouble(value);
    else if constexpr (std::is_same_v<U, std::string>) gen->SetReturnObject(const_cast<std::string*>(&value));
    else if constexpr (std::is_same_v<U, std::string_view>) {
        static thread_local std::string s;
        s = std::string(value);
        gen->SetReturnObject(&s);
    } else if constexpr (std::is_same_v<U, const char*>) {
        static thread_local std::string s;
        s = value ? value : "";
        gen->SetReturnObject(&s);
    } else if constexpr (SEIsAdoptPtr<U>::value) {
        using P = std::remove_cv_t<decltype(*value.ptr)>;
        if (value.ptr != nullptr) sm_angelOwnedObjects[value.ptr] = { 1, [](void* p) { delete static_cast<P*>(p); } };
        gen->SetReturnObject(value.ptr);
    } else if constexpr (SEIsSharedPtr<U>::value) {
        using P = std::remove_cv_t<typename U::element_type>;
        P* ptr = value.get();
        sm_angelSharedKeepAlive.emplace_back(std::static_pointer_cast<void>(value));
        gen->SetReturnObject(ptr);
    } else if constexpr (std::is_pointer_v<U>) gen->SetReturnObject(const_cast<std::remove_pointer_t<U>*>(value));
    else if constexpr (std::is_class_v<U>) gen->SetReturnObject(angelOwnedFactoryValue<U>(value));
    else static_assert(SEAlwaysFalse<T>, "Unsupported AngelScript return type");
}

template<typename T>
void ScriptEngine::angelSetContextArg(asIScriptContext* ctx, int idx, T&& value) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, bool>) ctx->SetArgByte(idx, value ? 1 : 0);
    else if constexpr (std::is_same_v<U, int>) ctx->SetArgDWord(idx, static_cast<asDWORD>(value));
    else if constexpr (std::is_same_v<U, int64_t>) ctx->SetArgQWord(idx, static_cast<asQWORD>(value));
    else if constexpr (std::is_same_v<U, float>) ctx->SetArgFloat(idx, value);
    else if constexpr (std::is_same_v<U, double>) ctx->SetArgDouble(idx, value);
    else if constexpr (std::is_same_v<U, std::string>) ctx->SetArgObject(idx, const_cast<std::string*>(&value));
    else if constexpr (std::is_same_v<U, std::string_view>) {
        static thread_local std::unordered_map<int, std::string> stringsByArg;
        stringsByArg[idx] = std::string(value);
        ctx->SetArgObject(idx, &stringsByArg[idx]);
    } else if constexpr (std::is_same_v<U, const char*>) {
        static thread_local std::unordered_map<int, std::string> stringsByArg;
        stringsByArg[idx] = value ? value : "";
        ctx->SetArgObject(idx, &stringsByArg[idx]);
    } else if constexpr (SEIsSharedPtr<U>::value) ctx->SetArgObject(idx, value.get());
    else if constexpr (std::is_pointer_v<U>) ctx->SetArgObject(idx, const_cast<std::remove_pointer_t<U>*>(value));
    else if constexpr (std::is_class_v<U>) ctx->SetArgObject(idx, const_cast<U*>(&value));
    else static_assert(SEAlwaysFalse<T>, "Unsupported AngelScript context argument type");
}

template<typename T>
T* ScriptEngine::angelOwnedFactoryValue(const T& value) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    U* ptr = new U(value);
    sm_angelOwnedObjects[ptr] = { 1, [](void* p) { delete static_cast<U*>(p); } };
    return ptr;
}

template<typename T>
void ScriptEngine::angelAddRef(T* ptr) {
    auto it = sm_angelOwnedObjects.find(ptr);
    if (it != sm_angelOwnedObjects.end()) ++it->second.m_refCount;
}

template<typename T>
void ScriptEngine::angelRelease(T* ptr) {
    auto it = sm_angelOwnedObjects.find(ptr);
    if (it == sm_angelOwnedObjects.end()) return;
    it->second.m_refCount -= 1;
    if (it->second.m_refCount > 0) return;
    auto deleter = it->second.m_deleter;
    sm_angelOwnedObjects.erase(it);
    if (deleter != nullptr) deleter(ptr);
}

// invoke (Ret defaults to void)
template<typename Ret, typename... Args>
Ret ScriptEngine::invoke(Identifier scriptId, std::string_view functionName, Args... args) {
    auto langIt = sm_scriptLanguageById.find(scriptId);
    if (langIt != sm_scriptLanguageById.end() && langIt->second == ScriptLanguage::AngelScript) {
        asIScriptEngine* engine = getAngelEngineOrNull(scriptId, "invoke");
        asIScriptModule* module = engine != nullptr ? getAngelModuleOrNull(scriptId, "invoke") : nullptr;
        if (engine == nullptr || module == nullptr) {
            if constexpr (std::is_void_v<Ret>) return;
            else return Ret{};
        }

        const std::string functionNameString(functionName);
        const std::string decl = angelTypeName<Ret>() + " " + functionNameString + "(" + angelParameterList<Args...>() + ")";
        asIScriptFunction* func = module->GetFunctionByDecl(decl.c_str());
        if (func == nullptr) {
            func = module->GetFunctionByName(functionNameString.c_str());
        }
        if (func == nullptr) failScriptAction("AngelScript function not found: " + functionNameString);

        asIScriptContext* ctx = engine->CreateContext();
        if (ctx == nullptr) failScriptAction("Failed to create AngelScript context");
        if (ctx->Prepare(func) < 0) {
            ctx->Release();
            failScriptAction("Failed to prepare AngelScript function: " + functionNameString);
        }

        int argIndex = 0;
        (angelSetContextArg<Args>(ctx, argIndex++, std::forward<Args>(args)), ...);

        const Identifier previousScriptID = smt_activeScriptID;
        smt_activeScriptID = scriptId;
        const int rc = ctx->Execute();
        smt_activeScriptID = previousScriptID;
        if (rc != asEXECUTION_FINISHED) {
            std::string message = "AngelScript execution failed for function: " + functionNameString + " (rc=" + std::to_string(rc) + ")";
            if (rc == asEXECUTION_EXCEPTION) {
                message += " exception: ";
                message += ctx->GetExceptionString() ? ctx->GetExceptionString() : "<unknown>";
            }
            ctx->Release();
            failScriptAction(message);
        }

        if constexpr (std::is_void_v<Ret>) {
            ctx->Release();
            return;
        } else {
            Ret r = angelGetContextReturn<Ret>(ctx);
            ctx->Release();
            return r;
        }
    }

    lua_State* L = getStateOrNull(scriptId, "invoke");
    if (L == nullptr) {
        if constexpr (std::is_void_v<Ret>) return;
        else return Ret{};
    }
    const std::string functionNameString(functionName);
    lua_getglobal(L, functionNameString.c_str());
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); throw std::runtime_error("Function not found: " + functionNameString); }
    (push<Args>(L, args), ...);
    const Identifier previousScriptID = smt_activeScriptID;
    smt_activeScriptID = scriptId;
    if (lua_pcall(L, int(sizeof...(Args)), std::is_void_v<Ret> ? 0 : 1, 0) != LUA_OK) {
        smt_activeScriptID = previousScriptID;
        std::string err = lua_tostring(L, -1); lua_pop(L, 1); throw std::runtime_error(err);
    }
    smt_activeScriptID = previousScriptID;
    if constexpr (std::is_void_v<Ret>) { return; } else { Ret r = check<Ret>(L, -1); lua_pop(L, 1); return r; }
}

