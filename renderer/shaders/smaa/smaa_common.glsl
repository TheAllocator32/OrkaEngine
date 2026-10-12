#ifndef ORKA_SMAA_COMMON_GLSL
#define ORKA_SMAA_COMMON_GLSL

#define SMAA_CUSTOM_SL 1
#define SMAA_PRESET_HIGH 1
#define SMAA_RT_METRICS u_smaaRtMetrics

uniform vec4 u_smaaRtMetrics;

#define ORKA_SMAA_TEXCOORD(_uv) (_uv)

#if BGFX_SHADER_TYPE_FRAGMENT && (BGFX_SHADER_LANGUAGE_GLSL >= 130 || BX_PLATFORM_EMSCRIPTEN)
out vec4 orka_smaa_FragColor;
#   define ORKA_SMAA_FRAGCOLOR orka_smaa_FragColor
#else
#   define ORKA_SMAA_FRAGCOLOR gl_FragColor
#endif

#define float2 vec2
#define float3 vec3
#define float4 vec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4

#if BGFX_SHADER_LANGUAGE_GLSL
#   define bool2 bvec2
#   define bool3 bvec3
#   define bool4 bvec4
#endif

#if BGFX_SHADER_LANGUAGE_HLSL > 300 \
 || BGFX_SHADER_LANGUAGE_PSSL \
 || BGFX_SHADER_LANGUAGE_SPIRV \
 || BGFX_SHADER_LANGUAGE_METAL
#   define SMAATexture2D(_tex) BgfxSampler2D _tex
#else
#   define SMAATexture2D(_tex) sampler2D _tex
#endif

#define SMAATexturePass2D(_tex) _tex
#if BGFX_SHADER_LANGUAGE_HLSL > 300
// D3D rejects implicit-gradient samples inside SMAA's dynamic search loops.
#   define SMAASampleLevelZero(_tex, _coord) texture2DLod(_tex, _coord, 0.0)
#   define SMAASampleLevelZeroPoint(_tex, _coord) texture2DLod(_tex, _coord, 0.0)
#   define SMAASampleLevelZeroOffset(_tex, _coord, _offset) texture2DLodOffset(_tex, _coord, 0.0, _offset)
#else
#   define SMAASampleLevelZero(_tex, _coord) texture2D(_tex, _coord)
#   define SMAASampleLevelZeroPoint(_tex, _coord) texture2D(_tex, _coord)
#   define SMAASampleLevelZeroOffset(_tex, _coord, _offset) texture2D(_tex, (_coord) + vec2(_offset) * SMAA_RT_METRICS.xy)
#endif
#define SMAASample(_tex, _coord) texture2D(_tex, _coord)
#define SMAASamplePoint(_tex, _coord) texture2D(_tex, _coord)
#define SMAASampleOffset(_tex, _coord, _offset) texture2D(_tex, (_coord) + vec2(_offset) * SMAA_RT_METRICS.xy)
#define SMAA_FLATTEN
#define SMAA_BRANCH BRANCH

#define mad(_a, _b, _c) ((_a) * (_b) + (_c))
#define lerp(_a, _b, _t) mix(_a, _b, _t)
#define saturate(_a) clamp(_a, 0.0, 1.0)

#include "smaa_core.glsl"

#endif // ORKA_SMAA_COMMON_GLSL
