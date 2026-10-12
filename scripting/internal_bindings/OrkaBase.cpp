#ifndef ORKA_DISABLE_SCRIPTING
/*
 * File: OgreBase.cpp
 * 
 * This File registers Orka Base functions and classes to the ScriptEngine
 * 
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */


#include "../../primitives/core/MathTypes.hpp"
#include "Bindings.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

constexpr float  xg_piF = 3.14159265358979323846f;
constexpr double xg_pi = 3.14159265358979323846;

inline float m4fGet(const matrix4x4f& m, int col, int row) { return m[col][row]; }
inline double m4dGet(const matrix4x4d& m, int col, int row) { return m[col][row]; }
inline float m43fGet(const matrix4x3f& m, int col, int row) { return m[col][row]; }
inline double m43dGet(const matrix4x3d& m, int col, int row) { return m[col][row]; }

inline void m4fSet(matrix4x4f* m, int col, int row, float v) { (*m)[col][row] = v; }
inline void m4dSet(matrix4x4d* m, int col, int row, double v) { (*m)[col][row] = v; }
inline void m43fSet(matrix4x3f* m, int col, int row, float v) { (*m)[col][row] = v; }
inline void m43dSet(matrix4x3d* m, int col, int row, double v) { (*m)[col][row] = v; }

inline float dotV2f(const vector2df& a, const vector2df& b) { return glm::dot(a, b); }
inline float dotV3f(const vector3df& a, const vector3df& b) { return glm::dot(a, b); }
inline float dotV4f(const vector4df& a, const vector4df& b) { return glm::dot(a, b); }
inline double dotV2d(const vector2dd& a, const vector2dd& b) { return glm::dot(a, b); }
inline double dotV3d(const vector3dd& a, const vector3dd& b) { return glm::dot(a, b); }
inline double dotV4d(const vector4dd& a, const vector4dd& b) { return glm::dot(a, b); }

inline vector3df crossV3f(const vector3df& a, const vector3df& b) { return glm::cross(a, b); }
inline vector3dd crossV3d(const vector3dd& a, const vector3dd& b) { return glm::cross(a, b); }

inline float lengthV2f(const vector2df& v) { return glm::length(v); }
inline float lengthV3f(const vector3df& v) { return glm::length(v); }
inline float lengthV4f(const vector4df& v) { return glm::length(v); }
inline double lengthV2d(const vector2dd& v) { return glm::length(v); }
inline double lengthV3d(const vector3dd& v) { return glm::length(v); }
inline double lengthV4d(const vector4dd& v) { return glm::length(v); }

inline vector2df normalizeV2f(const vector2df& v) { return glm::normalize(v); }
inline vector3df normalizeV3f(const vector3df& v) { return glm::normalize(v); }
inline vector4df normalizeV4f(const vector4df& v) { return glm::normalize(v); }
inline vector2dd normalizeV2d(const vector2dd& v) { return glm::normalize(v); }
inline vector3dd normalizeV3d(const vector3dd& v) { return glm::normalize(v); }
inline vector4dd normalizeV4d(const vector4dd& v) { return glm::normalize(v); }

inline vector2df mixV2f(const vector2df& a, const vector2df& b, float t) { return glm::mix(a, b, t); }
inline vector3df mixV3f(const vector3df& a, const vector3df& b, float t) { return glm::mix(a, b, t); }
inline vector4df mixV4f(const vector4df& a, const vector4df& b, float t) { return glm::mix(a, b, t); }
inline vector2dd mixV2d(const vector2dd& a, const vector2dd& b, double t) { return glm::mix(a, b, t); }
inline vector3dd mixV3d(const vector3dd& a, const vector3dd& b, double t) { return glm::mix(a, b, t); }
inline vector4dd mixV4d(const vector4dd& a, const vector4dd& b, double t) { return glm::mix(a, b, t); }

inline matrix4x4f mulM4f(const matrix4x4f& A, const matrix4x4f& B) { return A * B; }
inline matrix4x4d mulM4d(const matrix4x4d& A, const matrix4x4d& B) { return A * B; }
inline vector4df mulM4fV4f(const matrix4x4f& M, const vector4df& v) { return M * v; }
inline vector4dd mulM4dV4d(const matrix4x4d& M, const vector4dd& v) { return M * v; }

inline matrix4x4f transposeM4f(const matrix4x4f& M) { return glm::transpose(M); }
inline matrix4x4d transposeM4d(const matrix4x4d& M) { return glm::transpose(M); }
inline matrix4x4f inverseM4f(const matrix4x4f& M) { return glm::inverse(M); }
inline matrix4x4d inverseM4d(const matrix4x4d& M) { return glm::inverse(M); }
inline float  determinantM4f(const matrix4x4f& M) { return glm::determinant(M); }
inline double determinantM4d(const matrix4x4d& M) { return glm::determinant(M); }

inline quaternionf quatFromAxisAngleF(const vector3df& axis, float angleRadians) { return glm::angleAxis(angleRadians, glm::normalize(axis)); }
inline quaterniond quatFromAxisAngleD(const vector3dd& axis, double angleRadians) { return glm::angleAxis(angleRadians, glm::normalize(axis)); }
inline quaternionf quatMulF(const quaternionf& a, const quaternionf& b) { return a * b; }
inline quaterniond quatMulD(const quaterniond& a, const quaterniond& b) { return a * b; }
inline vector3df quatRotateF(const quaternionf& q, const vector3df& v) { return q * v; }
inline vector3dd quatRotateD(const quaterniond& q, const vector3dd& v) { return q * v; }
inline quaternionf quatNormalizeF(const quaternionf& q) { return glm::normalize(q); }
inline quaterniond quatNormalizeD(const quaterniond& q) { return glm::normalize(q); }

namespace ScriptBindings {
    inline vector3df ng_defaultForward{ 0.f, 0.f, -1.f };
    inline vector3df ng_defaultUp{ 0.f, 1.f,  0.f };
    inline matrix4x4f ng_identityM4{ 1.f };

    void registerOrkaBase() {
        ScriptEngine::addGlobalVariable("PI", xg_pi);
        ScriptEngine::addGlobalVariable("PI_F", xg_piF);

        ScriptEngine::addGlobalVariableRef("FORWARD", &ng_defaultForward);
        ScriptEngine::addGlobalVariableRef("UP", &ng_defaultUp);
        ScriptEngine::addGlobalVariableRef("IDENTITY_MAT4", &ng_identityM4);

        ScriptEngine::beginClass<vector2df>("vector2df");
        ScriptEngine::addConstructor<vector2df>();
        ScriptEngine::addConstructor<vector2df, float, float>();
        ScriptEngine::addConstructor<vector2df, float>();
        ScriptEngine::addProperty<vector2df, float>("x", &vector2df::x);
        ScriptEngine::addProperty<vector2df, float>("y", &vector2df::y);
        ScriptEngine::endClass<vector2df>();

        ScriptEngine::beginClass<vector2dd>("vector2dd");
        ScriptEngine::addConstructor<vector2dd>();
        ScriptEngine::addConstructor<vector2dd, double, double>();
        ScriptEngine::addConstructor<vector2dd, double>();
        ScriptEngine::addProperty<vector2dd, double>("x", &vector2dd::x);
        ScriptEngine::addProperty<vector2dd, double>("y", &vector2dd::y);
        ScriptEngine::endClass<vector2dd>();

        ScriptEngine::beginClass<vector2di>("vector2di");
        ScriptEngine::addConstructor<vector2di>();
        ScriptEngine::addConstructor<vector2di, int, int>();
        ScriptEngine::addConstructor<vector2di, int>();
        ScriptEngine::addProperty<vector2di, int>("x", &vector2di::x);
        ScriptEngine::addProperty<vector2di, int>("y", &vector2di::y);
        ScriptEngine::endClass<vector2di>();

        ScriptEngine::beginClass<vector3df>("vector3df");
        ScriptEngine::addConstructor<vector3df>();
        ScriptEngine::addConstructor<vector3df, float, float, float>();
        ScriptEngine::addConstructor<vector3df, float>();
        ScriptEngine::addProperty<vector3df, float>("x", &vector3df::x);
        ScriptEngine::addProperty<vector3df, float>("y", &vector3df::y);
        ScriptEngine::addProperty<vector3df, float>("z", &vector3df::z);
        ScriptEngine::endClass<vector3df>();

        ScriptEngine::beginClass<vector3dd>("vector3dd");
        ScriptEngine::addConstructor<vector3dd>();
        ScriptEngine::addConstructor<vector3dd, double, double, double>();
        ScriptEngine::addConstructor<vector3dd, double>();
        ScriptEngine::addProperty<vector3dd, double>("x", &vector3dd::x);
        ScriptEngine::addProperty<vector3dd, double>("y", &vector3dd::y);
        ScriptEngine::addProperty<vector3dd, double>("z", &vector3dd::z);
        ScriptEngine::endClass<vector3dd>();

        ScriptEngine::beginClass<vector3di>("vector3di");
        ScriptEngine::addConstructor<vector3di>();
        ScriptEngine::addConstructor<vector3di, int, int, int>();
        ScriptEngine::addConstructor<vector3di, int>();
        ScriptEngine::addProperty<vector3di, int>("x", &vector3di::x);
        ScriptEngine::addProperty<vector3di, int>("y", &vector3di::y);
        ScriptEngine::addProperty<vector3di, int>("z", &vector3di::z);
        ScriptEngine::endClass<vector3di>();

        ScriptEngine::beginClass<vector4df>("vector4df");
        ScriptEngine::addConstructor<vector4df>();
        ScriptEngine::addConstructor<vector4df, float, float, float, float>();
        ScriptEngine::addConstructor<vector4df, float>();
        ScriptEngine::addProperty<vector4df, float>("x", &vector4df::x);
        ScriptEngine::addProperty<vector4df, float>("y", &vector4df::y);
        ScriptEngine::addProperty<vector4df, float>("z", &vector4df::z);
        ScriptEngine::addProperty<vector4df, float>("w", &vector4df::w);
        ScriptEngine::endClass<vector4df>();

        ScriptEngine::beginClass<vector4dd>("vector4dd");
        ScriptEngine::addConstructor<vector4dd>();
        ScriptEngine::addConstructor<vector4dd, double, double, double, double>();
        ScriptEngine::addConstructor<vector4dd, double>();
        ScriptEngine::addProperty<vector4dd, double>("x", &vector4dd::x);
        ScriptEngine::addProperty<vector4dd, double>("y", &vector4dd::y);
        ScriptEngine::addProperty<vector4dd, double>("z", &vector4dd::z);
        ScriptEngine::addProperty<vector4dd, double>("w", &vector4dd::w);
        ScriptEngine::endClass<vector4dd>();

        ScriptEngine::beginClass<vector4di>("vector4di");
        ScriptEngine::addConstructor<vector4di>();
        ScriptEngine::addConstructor<vector4di, int, int, int, int>();
        ScriptEngine::addConstructor<vector4di, int>();
        ScriptEngine::addProperty<vector4di, int>("x", &vector4di::x);
        ScriptEngine::addProperty<vector4di, int>("y", &vector4di::y);
        ScriptEngine::addProperty<vector4di, int>("z", &vector4di::z);
        ScriptEngine::addProperty<vector4di, int>("w", &vector4di::w);
        ScriptEngine::endClass<vector4di>();

        ScriptEngine::beginClass<quaternionf>("quaternionf");
        ScriptEngine::addConstructor<quaternionf>();
        ScriptEngine::addConstructor<quaternionf, float, float, float, float>(); // (w,x,y,z)
        ScriptEngine::addProperty<quaternionf, float>("w", &quaternionf::w);
        ScriptEngine::addProperty<quaternionf, float>("x", &quaternionf::x);
        ScriptEngine::addProperty<quaternionf, float>("y", &quaternionf::y);
        ScriptEngine::addProperty<quaternionf, float>("z", &quaternionf::z);
        ScriptEngine::endClass<quaternionf>();

        ScriptEngine::beginClass<quaterniond>("quaterniond");
        ScriptEngine::addConstructor<quaterniond>();
        ScriptEngine::addConstructor<quaterniond, double, double, double, double>();
        ScriptEngine::addProperty<quaterniond, double>("w", &quaterniond::w);
        ScriptEngine::addProperty<quaterniond, double>("x", &quaterniond::x);
        ScriptEngine::addProperty<quaterniond, double>("y", &quaterniond::y);
        ScriptEngine::addProperty<quaterniond, double>("z", &quaterniond::z);
        ScriptEngine::endClass<quaterniond>();

        ScriptEngine::beginClass<matrix4x4f>("matrix4x4f");
        ScriptEngine::addConstructor<matrix4x4f>();     // identity
        ScriptEngine::addConstructor<matrix4x4f, float>(); // diagonal scalar
        ScriptEngine::endClass<matrix4x4f>();

        ScriptEngine::beginClass<matrix4x4d>("matrix4x4d");
        ScriptEngine::addConstructor<matrix4x4d>();
        ScriptEngine::addConstructor<matrix4x4d, double>();
        ScriptEngine::endClass<matrix4x4d>();

        ScriptEngine::beginClass<matrix4x3f>("matrix4x3f");
        ScriptEngine::addConstructor<matrix4x3f>();
        ScriptEngine::endClass<matrix4x3f>();

        ScriptEngine::beginClass<matrix4x3d>("matrix4x3d");
        ScriptEngine::addConstructor<matrix4x3d>();
        ScriptEngine::endClass<matrix4x3d>();

        ScriptEngine::addGlobalFunction("dot_v2f", &dotV2f);
        ScriptEngine::addGlobalFunction("dot_v3f", &dotV3f);
        ScriptEngine::addGlobalFunction("dot_v4f", &dotV4f);
        ScriptEngine::addGlobalFunction("dot_v2d", &dotV2d);
        ScriptEngine::addGlobalFunction("dot_v3d", &dotV3d);
        ScriptEngine::addGlobalFunction("dot_v4d", &dotV4d);

        ScriptEngine::addGlobalFunction("cross_v3f", &crossV3f);
        ScriptEngine::addGlobalFunction("cross_v3d", &crossV3d);

        ScriptEngine::addGlobalFunction("length_v2f", &lengthV2f);
        ScriptEngine::addGlobalFunction("length_v3f", &lengthV3f);
        ScriptEngine::addGlobalFunction("length_v4f", &lengthV4f);
        ScriptEngine::addGlobalFunction("length_v2d", &lengthV2d);
        ScriptEngine::addGlobalFunction("length_v3d", &lengthV3d);
        ScriptEngine::addGlobalFunction("length_v4d", &lengthV4d);

        ScriptEngine::addGlobalFunction("normalize_v2f", &normalizeV2f);
        ScriptEngine::addGlobalFunction("normalize_v3f", &normalizeV3f);
        ScriptEngine::addGlobalFunction("normalize_v4f", &normalizeV4f);
        ScriptEngine::addGlobalFunction("normalize_v2d", &normalizeV2d);
        ScriptEngine::addGlobalFunction("normalize_v3d", &normalizeV3d);
        ScriptEngine::addGlobalFunction("normalize_v4d", &normalizeV4d);

        ScriptEngine::addGlobalFunction("mix_v2f", &mixV2f);
        ScriptEngine::addGlobalFunction("mix_v3f", &mixV3f);
        ScriptEngine::addGlobalFunction("mix_v4f", &mixV4f);
        ScriptEngine::addGlobalFunction("mix_v2d", &mixV2d);
        ScriptEngine::addGlobalFunction("mix_v3d", &mixV3d);
        ScriptEngine::addGlobalFunction("mix_v4d", &mixV4d);

        ScriptEngine::addGlobalFunction("mul_m4f", &mulM4f);
        ScriptEngine::addGlobalFunction("mul_m4d", &mulM4d);
        ScriptEngine::addGlobalFunction("mul_m4f_v4f", &mulM4fV4f);
        ScriptEngine::addGlobalFunction("mul_m4d_v4d", &mulM4dV4d);

        ScriptEngine::addGlobalFunction("transpose_m4f", &transposeM4f);
        ScriptEngine::addGlobalFunction("transpose_m4d", &transposeM4d);
        ScriptEngine::addGlobalFunction("inverse_m4f", &inverseM4f);
        ScriptEngine::addGlobalFunction("inverse_m4d", &inverseM4d);
        ScriptEngine::addGlobalFunction("determinant_m4f", &determinantM4f);
        ScriptEngine::addGlobalFunction("determinant_m4d", &determinantM4d);

        ScriptEngine::addGlobalFunction("m4f_get", &m4fGet);
        ScriptEngine::addGlobalFunction("m4d_get", &m4dGet);
        ScriptEngine::addGlobalFunction("m43f_get", &m43fGet);
        ScriptEngine::addGlobalFunction("m43d_get", &m43dGet);

        ScriptEngine::addGlobalFunction("m4f_set", &m4fSet);
        ScriptEngine::addGlobalFunction("m4d_set", &m4dSet);
        ScriptEngine::addGlobalFunction("m43f_set", &m43fSet);
        ScriptEngine::addGlobalFunction("m43d_set", &m43dSet);

        ScriptEngine::addGlobalFunction("quat_from_axis_angle_f", &quatFromAxisAngleF);
        ScriptEngine::addGlobalFunction("quat_from_axis_angle_d", &quatFromAxisAngleD);
        ScriptEngine::addGlobalFunction("quat_mul_f", &quatMulF);
        ScriptEngine::addGlobalFunction("quat_mul_d", &quatMulD);
        ScriptEngine::addGlobalFunction("quat_rotate_f", &quatRotateF);
        ScriptEngine::addGlobalFunction("quat_rotate_d", &quatRotateD);
        ScriptEngine::addGlobalFunction("quat_normalize_f", &quatNormalizeF);
        ScriptEngine::addGlobalFunction("quat_normalize_d", &quatNormalizeD);
    }
}


#endif

