#ifndef ORKA_DISABLE_RENDERER

#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#define TINYGLTF3_ENABLE_STB_IMAGE
#define TINYGLTF3_ENABLE_STB_IMAGE_WRITE
#define TINYGLTF_JSON_USE_SIMD
#if defined(__AVX2__)
#define TINYGLTF3_JSON_SIMD_AVX2
#define TINYGLTF_JSON_SIMD_AVX2
#elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define TINYGLTF3_JSON_SIMD_SSE2
#define TINYGLTF_JSON_SIMD_SSE2
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define TINYGLTF3_JSON_SIMD_NEON
#define TINYGLTF_JSON_SIMD_NEON
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ext_tiny_gltf_v3.h"
#include "ext_stb_image.h"
#include "ext_stb_image_write.h"

#endif // ORKA_DISABLE_RENDERER
