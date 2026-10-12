# This file imports all required orka dependencies libraries
# Copyright (c) 2024-2025 Lucas G. (TheAllocator32)

option(ENABLE_DEBUG_INFO "Enable debug information in release builds" OFF)
option(ORKA_HEADLESS "Build the networking/essentials-only headless runtime" OFF)
option(ORKA_ENABLE_AUDIO "Enable Orka audio subsystem" ON)
option(ORKA_ENABLE_PHYSICS "Enable Orka physics subsystem" ON)
option(ORKA_ENABLE_CRYPTO "Enable Orka crypto subsystem" ON)
option(ORKA_ENABLE_NETWORKING "Enable Orka networking subsystem" ON)
option(ORKA_ENABLE_LEVIATHAN "Enable Orka Leviathan networking module" ON)
option(ORKA_ENABLE_GUI "Enable Orka GUI subsystem" ON)
option(ORKA_ENABLE_GAMEPLAY "Enable Orka gameplay subsystem" ON)
option(ORKA_ENABLE_SCRIPTING "Enable Orka scripting subsystem" ON)
option(ORKA_ENABLE_LUA "Enable Lua scripting backend" ON)
option(ORKA_ENABLE_ANGELSCRIPT "Enable AngelScript scripting backend" ON)
option(ORKA_ENABLE_RENDERER_PBR "Enable Orka renderer PBR support" ON)
option(ORKA_ENABLE_RENDERER_SKINNING "Enable Orka renderer skinning support" ON)
option(ORKA_ENABLE_MESHOPT "Enable meshoptimizer decoding" ON)

if(ORKA_HEADLESS)
    message(STATUS "Orka headless mode: disabling renderer, scenes, window/input, audio, GUI, physics, gameplay, and scripting.")
    set(ORKA_ENABLE_AUDIO OFF CACHE BOOL "Enable Orka audio subsystem" FORCE)
    set(ORKA_ENABLE_PHYSICS OFF CACHE BOOL "Enable Orka physics subsystem" FORCE)
    set(ORKA_ENABLE_GUI OFF CACHE BOOL "Enable Orka GUI subsystem" FORCE)
    set(ORKA_ENABLE_GAMEPLAY OFF CACHE BOOL "Enable Orka gameplay subsystem" FORCE)
    set(ORKA_ENABLE_SCRIPTING OFF CACHE BOOL "Enable Orka scripting subsystem" FORCE)
    set(ORKA_ENABLE_LUA OFF CACHE BOOL "Enable Lua scripting backend" FORCE)
    set(ORKA_ENABLE_ANGELSCRIPT OFF CACHE BOOL "Enable AngelScript scripting backend" FORCE)
    set(ORKA_ENABLE_RENDERER_PBR OFF CACHE BOOL "Enable Orka renderer PBR support" FORCE)
    set(ORKA_ENABLE_RENDERER_SKINNING OFF CACHE BOOL "Enable Orka renderer skinning support" FORCE)
    set(ORKA_ENABLE_MESHOPT OFF CACHE BOOL "Enable meshoptimizer decoding" FORCE)
    list(APPEND ORKA_COMPILEFLAGS
        ORKA_HEADLESS
        ORKA_DISABLE_AUDIO
        ORKA_DISABLE_PHYSICS
        ORKA_DISABLE_GUI
        ORKA_DISABLE_GAMEPLAY
        ORKA_DISABLE_SCRIPTING
        ORKA_DISABLE_LUA
        ORKA_DISABLE_ANGELSCRIPT
        ORKA_DISABLE_RENDERER
        ORKA_DISABLE_SCENE
        ORKA_DISABLE_WINDOW
        ORKA_DISABLE_INPUT
    )
endif()

if(ORKA_ENABLE_LEVIATHAN AND NOT ORKA_ENABLE_NETWORKING)
    message(STATUS "ORKA_ENABLE_LEVIATHAN depends on ORKA_ENABLE_NETWORKING; enabling networking.")
    set(ORKA_ENABLE_NETWORKING ON CACHE BOOL "Enable Orka networking subsystem" FORCE)
endif()

if(ORKA_ENABLE_NETWORKING AND NOT ORKA_ENABLE_CRYPTO)
    message(STATUS "ORKA_ENABLE_NETWORKING depends on ORKA_ENABLE_CRYPTO; enabling crypto.")
    set(ORKA_ENABLE_CRYPTO ON CACHE BOOL "Enable Orka crypto subsystem" FORCE)
endif()

if(ORKA_ENABLE_GAMEPLAY AND NOT ORKA_ENABLE_PHYSICS)
    message(STATUS "ORKA_ENABLE_GAMEPLAY depends on ORKA_ENABLE_PHYSICS; enabling physics.")
    set(ORKA_ENABLE_PHYSICS ON CACHE BOOL "Enable Orka physics subsystem" FORCE)
endif()

if(ORKA_ENABLE_GAMEPLAY AND NOT ORKA_ENABLE_SCRIPTING)
    message(STATUS "ORKA_ENABLE_GAMEPLAY depends on ORKA_ENABLE_SCRIPTING; enabling scripting.")
    set(ORKA_ENABLE_SCRIPTING ON CACHE BOOL "Enable Orka scripting subsystem" FORCE)
endif()

if(ORKA_ENABLE_SCRIPTING AND NOT ORKA_ENABLE_LUA AND NOT ORKA_ENABLE_ANGELSCRIPT)
    message(STATUS "ORKA_ENABLE_SCRIPTING requires at least one backend; enabling Lua.")
    set(ORKA_ENABLE_LUA ON CACHE BOOL "Enable Lua scripting backend" FORCE)
endif()

if(NOT ORKA_HEADLESS)
    # bgfx
    find_package(bgfx CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${BGFX_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS bgfx::bx bgfx::bgfx bgfx::bimg)
endif()

if(ORKA_ENABLE_PHYSICS)
    # Bullet
    find_package(Bullet CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${BULLET_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS
        LinearMath
        BulletCollision
        BulletDynamics
        BulletSoftBody
        BulletInverseDynamics
        Bullet3Common
        ConvexDecomposition
    )
else()
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_PHYSICS)
endif()

if(NOT ORKA_HEADLESS)
    # SDL
    find_package(SDL3 CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${SDL3_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS ${SDL3_LIBRARIES})
endif()

# ZLIB
find_package(ZLIB REQUIRED)
list(APPEND ORKA_INCLUDES ${ZLIB_INCLUDE_DIRS})
list(APPEND ORKA_LIBS ZLIB::ZLIB)

# Minizip
find_package(minizip CONFIG REQUIRED)
list(APPEND ORKA_INCLUDES ${minizip_INCLUDE_DIRS})
list(APPEND ORKA_LIBS $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>)


if(ORKA_ENABLE_SCRIPTING)
    # The current ScriptEngine translation unit still references both backend APIs.
    # ORKA_ENABLE_LUA/ANGELSCRIPT select runtime availability; both packages are linked while scripting is enabled.
    find_package(Lua REQUIRED) # Or Lua CONFIG REQUIRED
    list(APPEND ORKA_INCLUDES ${LUA_INCLUDE_DIR})
    list(APPEND ORKA_LIBS ${LUA_LIBRARIES})

    if(NOT ORKA_ENABLE_LUA)
        list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_LUA)
    endif()

    if(ORKA_ENABLE_ANGELSCRIPT)
        find_package(Angelscript CONFIG QUIET)
        if(TARGET Angelscript::angelscript)
            list(APPEND ORKA_LIBS Angelscript::angelscript)
        elseif(TARGET angelscript)
            list(APPEND ORKA_LIBS angelscript)
        elseif(Angelscript_FOUND)
            message(WARNING "AngelScript package was found, but no known CMake target was exported. Disabling AngelScript backend.")
            list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_ANGELSCRIPT)
        else()
            message(WARNING "AngelScript package not found. Disabling AngelScript backend.")
            list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_ANGELSCRIPT)
        endif()
    else()
        list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_ANGELSCRIPT)
    endif()
else()
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_SCRIPTING ORKA_DISABLE_LUA ORKA_DISABLE_ANGELSCRIPT)
endif()

# Localisation
find_package(Boost REQUIRED COMPONENTS locale)
list(APPEND ORKA_INCLUDES ${BOOST_INCLUDE_DIRS})
list(APPEND ORKA_LIBS Boost::locale)

if(ORKA_ENABLE_CRYPTO)
    find_package(OpenSSL REQUIRED)
    list(APPEND ORKA_LIBS OpenSSL::SSL OpenSSL::Crypto)
else()
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_CRYPTO)
endif()

if(ORKA_ENABLE_GUI)
    # ImGUI
    find_package(imgui CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${IMGUI_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS imgui::imgui)

    # Implot
    find_package(implot CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${IMPLOT_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS implot::implot)

    # RmlUI
    find_package(RmlUi CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${RmlUi_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS RmlUi::RmlUi)
else()
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_GUI)
endif()

# GLM
find_package(glm CONFIG REQUIRED)
list(APPEND ORKA_INCLUDES ${glm_INCLUDE_DIRS})
list(APPEND ORKA_LIBS glm::glm)

if(NOT ORKA_HEADLESS)
    # TinyEXR
    find_package(tinyexr CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${tinyexr_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS unofficial::tinyexr::tinyexr)
endif()

if(ORKA_ENABLE_MESHOPT)
    # MeshOptimizer
    find_package(meshoptimizer CONFIG REQUIRED)
    list(APPEND ORKA_INCLUDES ${meshoptimizer_INCLUDE_DIRS})
    list(APPEND ORKA_LIBS meshoptimizer::meshoptimizer)
else()
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_MESHOPT)
endif()

# Compiler Settings
if(ORKA_ENABLE_NETWORKING)
    list(APPEND ORKA_COMPILEFLAGS CPPHTTPLIB_OPENSSL_SUPPORT)
else()
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_NETWORKING)
endif()

if(NOT ORKA_ENABLE_LEVIATHAN)
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_LEVIATHAN)
endif()

if(NOT ORKA_ENABLE_AUDIO)
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_AUDIO)
endif()

if(NOT ORKA_ENABLE_GAMEPLAY)
    list(APPEND ORKA_COMPILEFLAGS ORKA_DISABLE_GAMEPLAY)
endif()

if(NOT ORKA_ENABLE_RENDERER_PBR)
    list(APPEND ORKA_COMPILEFLAGS ORKA_RENDERER_DISABLE_PBR)
endif()

if(NOT ORKA_ENABLE_RENDERER_SKINNING)
    list(APPEND ORKA_COMPILEFLAGS ORKA_RENDERER_DISABLE_SKINNING)
endif()

list(APPEND ORKA_INCLUDES "src/orka")

list(REMOVE_DUPLICATES ORKA_INCLUDES)
list(REMOVE_DUPLICATES ORKA_LIBS)
list(REMOVE_DUPLICATES ORKA_COMPILEFLAGS)

# Shader code generation (renderer/shaders/** -> *.autogenerated.shader.hpp)
set(_orka_base_dir "")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/renderer/shaders" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/shaderc/OrkaShaderC.cmake")
    set(_orka_base_dir "${CMAKE_CURRENT_LIST_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/src/orka/renderer/shaders" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/src/orka/shaderc/OrkaShaderC.cmake")
    set(_orka_base_dir "${CMAKE_CURRENT_LIST_DIR}/src/orka")
endif()

if(NOT ORKA_HEADLESS AND NOT _orka_base_dir STREQUAL "")
    include("${_orka_base_dir}/shaderc/OrkaShaderC.cmake")
    set(ORKA_SHADER2CODE_TARGET "orka_shader2code_renderer_shaders")
    shader2code(
        ROOT_DIR "${_orka_base_dir}/renderer/shaders"
        TARGET "${ORKA_SHADER2CODE_TARGET}"
    )
else()
    message(STATUS "Orka ShaderC not found (expected at shaderc/ under orka root). Skipping auto shader2code for renderer/shaders.")
endif()
