# Compiler/platform setup that must run before project() for reliable toolchain detection.
# Copyright (c) 2024-2026 Lucas G. (TheAllocator32)

include_guard(GLOBAL)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
elseif(CMAKE_CXX_STANDARD LESS 23)
    message(FATAL_ERROR "Orka requires C++23 or newer!! Set CMAKE_CXX_STANDARD to 23 or higher before including orka.compilehacks.cmake.")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This file runs before project(), so CMAKE_SYSTEM_PROCESSOR may not have been
# initialized yet. Prefer an explicitly selected generator architecture, then
# the target and host processor names.
set(ORKA_SYSTEM_PROCESSOR "${CMAKE_GENERATOR_PLATFORM}")
if(NOT ORKA_SYSTEM_PROCESSOR)
    set(ORKA_SYSTEM_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(NOT ORKA_SYSTEM_PROCESSOR)
    set(ORKA_SYSTEM_PROCESSOR "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${ORKA_SYSTEM_PROCESSOR}" ORKA_SYSTEM_PROCESSOR)
if(ORKA_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|armv8(a|_a)?|armv8.*64)$")
    set(ORKA_VCPKG_ARCH "arm64")
elseif(ORKA_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|x64)$")
    set(ORKA_VCPKG_ARCH "x64")
elseif(ORKA_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
    set(ORKA_VCPKG_ARCH "x86")
elseif(ORKA_SYSTEM_PROCESSOR MATCHES "^(arm|armv[5-8].*|armv[5-7].*)$")
    set(ORKA_VCPKG_ARCH "arm")
endif()

if(WIN32)
    if(ENABLE_DEBUG_INFO)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
    endif()

    if(ORKA_VCPKG_ARCH MATCHES "^(x64|x86|arm64)$")
        set(ORKA_DEFAULT_VCPKG_TRIPLET "${ORKA_VCPKG_ARCH}-windows-static")
    endif()
    set(VCPKG_LIBRARY_LINKAGE "static")

        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zc:__cplusplus /std:c++23")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zc:__cplusplus /std:c++23")

elseif(APPLE)
    if(ENABLE_DEBUG_INFO)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
    endif()

    if(ORKA_VCPKG_ARCH MATCHES "^(x64|arm64)$")
        set(ORKA_DEFAULT_VCPKG_TRIPLET "${ORKA_VCPKG_ARCH}-osx")
    endif()
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)

elseif(UNIX)
    if(ENABLE_DEBUG_INFO)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
    endif()

    if(ORKA_VCPKG_ARCH MATCHES "^(x64|x86|arm64|arm)$")
        set(ORKA_DEFAULT_VCPKG_TRIPLET "${ORKA_VCPKG_ARCH}-linux")
    endif()
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)

else()
    message(WARNING "Currently Unsupported platform! Proceeding anyway..")
endif()

# Unknown processor names (for example, MIPS, AVR, RISC-V, or PowerPC) require a custom defined VCPKG_TARGET_TRIPLET
if(NOT DEFINED VCPKG_TARGET_TRIPLET OR VCPKG_TARGET_TRIPLET STREQUAL "")
    if(NOT ORKA_DEFAULT_VCPKG_TRIPLET)
        message(FATAL_ERROR
            "Cannot select a vcpkg triplet for processor '${ORKA_SYSTEM_PROCESSOR}'. "
            "Set VCPKG_TARGET_TRIPLET to a suitable built-in or custom triplet before configuring.")
    endif()
    set(VCPKG_TARGET_TRIPLET "${ORKA_DEFAULT_VCPKG_TRIPLET}" CACHE STRING "vcpkg target triplet")
endif()
