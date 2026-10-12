# Configuration flags

Compile-time flags must be defined through CMake before compiling any translation unit that includes Orka headers. Orka Wave Breaker (tm) writes them into the generated project's `CMakeLists.txt` with `target_compile_definitions(...)`.


| CMake option | Default | Compile definition when disabled | Function |
| --- | --- | --- | --- |
| `ORKA_HEADLESS` | `OFF` | `ORKA_HEADLESS` | Networking/essentials-only runtime; disables renderer, scenes, window/input, audio, GUI, physics, gameplay, and scripting |
| `ORKA_ENABLE_AUDIO` | `ON` | `ORKA_DISABLE_AUDIO` | Audio subsystem and music helpers |
| `ORKA_ENABLE_PHYSICS` | `ON` | `ORKA_DISABLE_PHYSICS` | Bullet physics subsystem |
| `ORKA_ENABLE_CRYPTO` | `ON` | `ORKA_DISABLE_CRYPTO` | Crypto sessions, hashes, and encryption helpers |
| `ORKA_ENABLE_NETWORKING` | `ON` | `ORKA_DISABLE_NETWORKING` | HTTP and realtime networking |
| `ORKA_ENABLE_LEVIATHAN` | `ON` | `ORKA_DISABLE_LEVIATHAN` | Leviathan client and relay server |
| `ORKA_ENABLE_GUI` | `ON` | `ORKA_DISABLE_GUI` | ImGui/RmlUi GUI subsystem |
| `ORKA_ENABLE_GAMEPLAY` | `ON` | `ORKA_DISABLE_GAMEPLAY` | GameMode, Player, upgrades, character controller |
| `ORKA_ENABLE_SCRIPTING` | `ON` | `ORKA_DISABLE_SCRIPTING` | Script engine |
| `ORKA_ENABLE_LUA` | `ON` | `ORKA_DISABLE_LUA` | Lua scripting backend |
| `ORKA_ENABLE_ANGELSCRIPT` | `ON` | `ORKA_DISABLE_ANGELSCRIPT` | AngelScript scripting backend |
| `ORKA_ENABLE_RENDERER_PBR` | `ON` | `ORKA_RENDERER_DISABLE_PBR` | PBR material/shader path |
| `ORKA_ENABLE_RENDERER_SKINNING` | `ON` | `ORKA_RENDERER_DISABLE_SKINNING` | Skeleton/bone/skinning path |
| `ORKA_ENABLE_MESHOPT` | `ON` | `ORKA_DISABLE_MESHOPT` | meshoptimizer decoding in the GLTF loader |

## Direct compile definitions

If you are not using the generated CMake options, add these manually with `target_compile_definitions(...)`:

| Flag | Function |
| --- | --- |
| `ORKA_RENDERER_DISABLE_SKINNING` | Disables skeleton/bone/skinning data paths |
| `ORKA_RENDERER_DISABLE_PBR` | Disables PBR material/shader data paths |
| `ORKA_DISABLE_PHYSICS` | Disables physics subsystem hooks |
| `ORKA_DISABLE_MESHOPT` | Disables meshoptimizer usage in GLTF loading |
| `ORKA_DISABLE_AUDIO` | Disables automatic audio init/update/deinit in `OrkaEngine` |
| `ORKA_DISABLE_CRYPTO` | Excludes crypto from the umbrella header and CMake dependencies |
| `ORKA_DISABLE_NETWORKING` | Disables automatic realtime network init/deinit and networking header exports |
| `ORKA_DISABLE_LEVIATHAN` | Excludes Leviathan client/server implementation and umbrella-header exports |
| `ORKA_DISABLE_GUI` | Disables automatic GUI init/update and GUI header exports |
| `ORKA_DISABLE_GAMEPLAY` | Excludes gameplay headers and current `GameMode` helpers |
| `ORKA_DISABLE_SCRIPTING` | Disables automatic script engine init/deinit and script header exports |
| `ORKA_DISABLE_LUA` | Marks Lua scripting as disabled |
| `ORKA_DISABLE_ANGELSCRIPT` | Marks AngelScript scripting as disabled |
| `ORKA_HEADLESS` | Excludes output-dependent systems, their source files, and graphics/window dependencies while retaining essentials, WhaleML, crypto, networking, and Leviathan |
