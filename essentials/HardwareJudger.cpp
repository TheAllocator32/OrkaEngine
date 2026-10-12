/*
 * File: HardwareJudger.cpp
 *
 * See HardwareJudger.hpp - I'm just too lazy to repeat myself here :/
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "HardwareJudger.hpp"
#include "../utilities/Tools.hpp"
#include "Logger.hpp"
#include <thread>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#ifndef ORKA_HEADLESS
#include <bgfx/bgfx.h>
#include "../renderer/Renderer.hpp"
#endif

#ifdef __linux__
#include <unistd.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#include <cpuid.h>
#endif
#endif

#define PING_TEMP_FILE std::string(MiscTools::getGameUserDataPath() + "hardware_judger_ping.sacredfile.txt")

#ifdef _WIN32
static std::string trimHardwareString(std::string value) {
    while (!value.empty()) {
        const unsigned char c = static_cast<unsigned char>(value.back());
        if (c != '\0' && !std::isspace(c)) break;
        value.pop_back();
    }

    size_t first = 0;
    while (first < value.size()) {
        const unsigned char c = static_cast<unsigned char>(value[first]);
        if (c != '\0' && !std::isspace(c)) break;
        ++first;
    }

    if (first > 0) {
        value.erase(0, first);
    }
    return value;
}

static bool queryCpuid(unsigned int leaf, std::array<int, 4>& regs) {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    __cpuid(regs.data(), static_cast<int>(leaf));
    return true;
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    if (__get_cpuid(leaf, &eax, &ebx, &ecx, &edx) == 0) {
        return false;
    }
    regs[0] = static_cast<int>(eax);
    regs[1] = static_cast<int>(ebx);
    regs[2] = static_cast<int>(ecx);
    regs[3] = static_cast<int>(edx);
    return true;
#else
    (void)leaf;
    (void)regs;
    return false;
#endif
}

static std::string getWindowsCpuModelName() {
    std::array<int, 4> regs{};
    if (queryCpuid(0x80000000u, regs) && static_cast<unsigned int>(regs[0]) >= 0x80000004u) {
        char brand[49] = {};
        for (unsigned int leaf = 0x80000002u; leaf <= 0x80000004u; ++leaf) {
            if (!queryCpuid(leaf, regs)) {
                break;
            }
            std::memcpy(brand + ((leaf - 0x80000002u) * sizeof(regs)), regs.data(), sizeof(regs));
        }

        std::string name = trimHardwareString(std::string(brand, 48));
        if (!name.empty()) {
            return name;
        }
    }

    const char* processorIdentifier = std::getenv("PROCESSOR_IDENTIFIER");
    if (processorIdentifier != nullptr && processorIdentifier[0] != '\0') {
        return processorIdentifier;
    }
    return "Unknown";
}
#endif

int architectureBonusMalus() {
    #if defined(__x86_64__) || defined(_M_X64)
        g_hardwareJudgement.comments.push_back("Hmmmm... Your Architecture and instruction set seem compatible... For now!");
        return 0;
    #elif defined(__aarch64__) || defined(_M_ARM64)
        g_hardwareJudgement.comments.push_back("ARM user detected! ARM works great on toasters and occasionally on Macs! ");
        return -1;
    #else
        g_hardwareJudgement.comments.push_back("What the hell? How did you get here? RISC-V? AVR? Nice try but you'll never get Host!");
        return -1000;
    #endif
}

int neutralCPUEvaluation() {
    int score = 0;

    unsigned int cores = std::thread::hardware_concurrency();

    if (cores == 0) {
        g_hardwareJudgement.comments.push_back("We couldn't detect your core count. Are you running this on a rock?");
        return -20;
    }

    std::string coresStr = std::to_string(cores) + " " + (cores == 1 ? "core" : "cores") + " - ";

    if (cores == 1) {
        g_hardwareJudgement.comments.push_back("Single-threaded sadness. Not even JavaScript deserves this!");
        score -= 10;
    } else if (cores == 2) {
        g_hardwareJudgement.comments.push_back("Two cores. Just enough to DOWNLOAD a game.");
        score -= 5;
    } else if (cores <= 4) {
        g_hardwareJudgement.comments.push_back("Quad-core or maybe some exotic 7 core CPU. Acceptable... in 2010.");
        score -= 2;
    } else if (cores <= 8) {
        g_hardwareJudgement.comments.push_back(coresStr + "You have enough threads to run a game. Barely.");
        score += 0;
    } else if (cores <= 12) {
        g_hardwareJudgement.comments.push_back(coresStr + "Your CPU seems serious about multitasking. Respectable.");
        score += 4;
    } else if (cores <= 16) {
        g_hardwareJudgement.comments.push_back(coresStr + "A solid CPU for gaming and feeling superior.");
        score += 6;
    } else if (cores <= 24) {
        g_hardwareJudgement.comments.push_back(coresStr + "You could probably run the game and simulate the weather at the same time.");
        score += 10;
    } else if (cores <= 32) {
        g_hardwareJudgement.comments.push_back(coresStr + "Who hurt you? This many cores aren't healthy.");
        score += 20;
    } else if (cores <= 64) {
        g_hardwareJudgement.comments.push_back(coresStr + "Okay, you’re not gaming. You’re rendering the next Pixar film.");
        score += 25;
    } else if (cores <= 80) {
        g_hardwareJudgement.comments.push_back(coresStr + "What the hell?? Hey, our Server has 20 cores and you Play our game on a 80+ Core CPU!?!");
        score += 40;
    } else {
        g_hardwareJudgement.comments.push_back(coresStr + "Congratulations! Your CPU is a war crime in some jurisdictions.");
        score += 70;
    }

    return score;
}

std::string getCpuModelName() {
#ifdef __linux__
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos)
            return line.substr(line.find(":") + 2);
    }
#elif __APPLE__
    char buffer[128];
    size_t size = sizeof(buffer);
    sysctlbyname("machdep.cpu.brand_string", &buffer, &size, nullptr, 0);
    return std::string(buffer);
#elif defined(_WIN32)
    return getWindowsCpuModelName();
#endif
    return "Unknown";
}

int processorNameBonusMalus() {
    std::string name = getCpuModelName();
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    int score = 0;

    // oof
    if (lower.find("atmega328p") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("ATmega328P - nice try. Go blink an LED. \n\t No wait... WHAT!?!?!? To be serious... I added this 'CPU' as a joke because why not... And you got Orka working there?? That's honestly an achievement! This message alone will exceed your RAM.");
        return -100000;
    }
    if (lower.find("tegra x1") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Tegra X1 - modern by Nintendo standards.");
        return -10000;
    }
    if (lower.find("celeron") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Celeron - slower than your grandma's handwriting.");
        return -100;
    }
    if (lower.find("atom") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Intel Atom - almost qualifies as a calculator.");
        return -200;
    }
    if (lower.find("pentium 4") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Pentium 4 - hot, slow, and very 2003.");
        return -300;
    }
    if (lower.find("pentium d") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Pentium D - dual-core in the most disappointing way.");
        return -400;
    }
    if (lower.find("phenom") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("AMD Phenom - not quite phenomenal.");
        return -50;
    }
    if (lower.find("fx-") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("AMD FX - 8 cores, 0 performance.");
        return -80;
    }
    if (lower.find("centrino") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Centrino - Intel's favorite sticker.");
        return -42;
    }
    if (lower.find("via c3") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("VIA C3 - yes, it technically runs code.");
        return -350;
    }
    if (lower.find("geode") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("AMD Geode - industrial chic, but don't expect frames.");
        return -100;
    }
    if (lower.find("arm1176") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("ARM1176 - powering 15-year-old Raspberries... barely.");
        return -500;
    }

    // Potato - mid range
    if (lower.find("i3-6100") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("i3-6100 - entry-level excellence back when YouTube had dislikes.");
        return -5;
    }
    if (lower.find("i5-2500k") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("i5-2500K - Sandy Bridge supremacy never dies.");
        return +2;
    }
    if (lower.find("i7-2600k") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("i7-2600K - the cockroach of CPUs: still alive, still kicking.");
        return +3;
    }
    if (lower.find("ryzen 5 1600") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Ryzen 5 1600 - the multi-threaded working-class hero.");
        return +5;
    }
    if (lower.find("xeon e5-2670") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Xeon E5-2670 - data center throwaway turned gaming budget legend.");
        return +4;
    }

    // Good - Crazy
    if (lower.find("threadripper") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Threadripper - built for rendering universes.");
        return +25;
    }
    if (lower.find("epyc") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("EPYC - you're not a gamer, you're a computational deity.");
        return +20;
    }
    if (lower.find("xeon platinum") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Xeon Platinum - this is not a gaming rig, it's a mainframe.");
        return +20;
    }
    if (lower.find("xeon phi") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Xeon Phi - technically a CPU, philosophically a mistake.");
        return -200;
    }
    if (lower.find("power9") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("POWER9 - open, fast, obscure. Just like your taste.");
        return +25;
    }
    if (lower.find("ampere") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Ampere Altra - ARM takes revenge on x86, one rack at a time.");
        return +15;
    }
    if (lower.find("apple m1") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Apple M1 - you escaped Intel. Good for you. Bad for our Physics Engine");
        return +12;
    }
    if (lower.find("apple m2") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Apple M2 - fast, efficient, and locked tighter than Fort Knox.");
        return +14;
    }
    if (lower.find("ryzen 9 5950x") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Ryzen 9 5950X - AMD's final flex before going 3D.");
        return +20;
    }
    if (lower.find("ryzen 9 7950x") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Ryzen 9 7950X - your socket is melting, but who cares?");
        return +25;
    }
    if (lower.find("i9-9900k") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("i9-9900K - the last good Intel chip before the chaos.");
        return +10;
    }
    if (lower.find("i9-12900k") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("i9-12900K - performance meets power draw, both excessively.");
        return +20;
    }
    if (lower.find("i9-13900k") != std::string::npos || lower.find("i9-14900k") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("i9-13900K/14900K - absurd speed, absurd heat, absurd bragging rights.");
        if (lower.find("i9-14900k") != std::string::npos) g_hardwareJudgement.comments.push_back("i9-14900K - the last good CPU before Intel went ultra :(");
        return +30;
    }

    // WTF!??!
    if (lower.find("zhaoxin") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Zhaoxin - proudly not quite VIA, not quite x86. But hey, it boots!");
        return -150;
    }
    if (lower.find("transmeta") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Transmeta Crusoe - You’re literally running on a software-defined CPU. How retro.");
        return -200;
    }
    if (lower.find("huawei") != std::string::npos || lower.find("kunpeng") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Huawei Kunpeng - ARM server power in your hands. Probably watched.");
        return +10;
    }
    if (lower.find("hisilicon") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("HiSilicon - respect, but unless you're in Shenzhen, this is weird.");
        return 0;
    }
    if (lower.find("rockchip") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Rockchip - the favorite SoC of budget Android tablets everywhere.");
        return -50;
    }
    if (lower.find("allwinner") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Allwinner - well, not *you*.");
        return -70;
    }
    if (lower.find("elbrus") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Elbrus - Russian VLIW CPU. Mysterious, slow, and glorious.");
        return -20;
    }
    if (lower.find("ingenic") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Ingenic - MIPS lives! But only just.");
        return -100;
    }
    if (lower.find("risc-v") != std::string::npos || lower.find("riscv") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("RISC-V - you compiled this yourself, didn’t you?");
        return -10;
    }
    if (lower.find("ti am") != std::string::npos || lower.find("omap") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("TI OMAP - you’re running a smartphone SoC from 2008. Bold.");
        return -80;
    }
    if (lower.find("powerpc") != std::string::npos || lower.find("power mac") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("PowerPC - Apple called. They want their beige G5 tower back.");
        return -90;
    }
    if (lower.find("ibm z") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("IBM Z-series - this isn’t just a CPU, it’s a cultural artifact.");
        return +30;
    }
    if (lower.find("ibm s") != std::string::npos && lower.find("390") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("IBM S/390 - mainframe detected. Please don't try to run Crysis.");
        return +40;
    }
    if (lower.find("sun ultrasparc") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Sun UltraSPARC - you’re not playing the game, you’re porting it.");
        return -10;
    }
    if (lower.find("mips") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("MIPS - it’s alive... sort of.");
        return -30;
    }
    if (lower.find("sparc") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("SPARC - exotic, ancient, probably running Solaris 10.");
        return -15;
    }
    if (lower.find("loongson") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("Loongson - homegrown, beautiful, incompatible with everything.");
        return -60;
    }
    if (lower.find("via") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("VIA CPU - your machine is legally a typewriter.");
        return -200;
    }
    if (lower.find("alphaserver") != std::string::npos || lower.find("ev6") != std::string::npos) {
        g_hardwareJudgement.comments.push_back("DEC Alpha - you’re either a museum curator or a time traveler.");
        return +42;
    }

    // Unknown CPU
    g_hardwareJudgement.comments.push_back("Your CPU evades categorization! Suspicious.");
    return 0;
}

malus_or_bonus HardwareJudger::judgeCPU() {
    int cpu=0;
    cpu += architectureBonusMalus();
    cpu += neutralCPUEvaluation();
    cpu += processorNameBonusMalus();
    return cpu;
}

#ifndef ORKA_HEADLESS
static std::string gpuVendorName(uint16_t vendorId) {
    switch (vendorId) {
        case 0x10DE: return "NVIDIA GPU";
        case 0x1002: return "AMD GPU";
        case 0x8086: return "Intel GPU";
        case 0x106B: return "Apple GPU";
        case 0x1414: return "Microsoft GPU";
        case 0x13B5: return "ARM GPU";
        case 0x1AF4: return "Software Rasterizer";
        default: return "";
    }
}

#ifdef __linux__
static uint16_t findLinuxGpuVendorId() {
    std::error_code error;
    uint16_t fallbackVendorId = 0;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm", error)) {
        const std::string name = entry.path().filename().string();
        if (!name.starts_with("card") || name.find('-') != std::string::npos) continue;

        std::ifstream vendorFile(entry.path() / "device" / "vendor");
        std::string vendor;
        if (!std::getline(vendorFile, vendor)) continue;

        try {
            const unsigned long parsedVendor = std::stoul(vendor, nullptr, 0);
            if (parsedVendor > UINT16_MAX) continue;

            const uint16_t vendorId = static_cast<uint16_t>(parsedVendor);
            if (fallbackVendorId == 0)
                fallbackVendorId = vendorId;

            std::ifstream bootVgaFile(entry.path() / "device" / "boot_vga");
            std::string bootVga;
            if (std::getline(bootVgaFile, bootVga) && bootVga == "1")
                return vendorId;
        }
        catch (...) {
        }
    }
    return fallbackVendorId;
}
#endif

malus_or_bonus HardwareJudger::judgeGPU() {
    const bgfx::Caps* caps = bgfx::getCaps();
    uint16_t vendorId = caps ? caps->vendorId : 0;
#ifdef __linux__
    if (gpuVendorName(vendorId).empty()) {
        const uint16_t linuxVendorId = findLinuxGpuVendorId();
        if (linuxVendorId != 0)
        vendorId = linuxVendorId;
    }
#endif

    std::string gpu = gpuVendorName(vendorId);
    if (gpu.empty()) {
        gpu = "Unknown GPU";
    }

    int gpuBonus = 0;
    std::string gpuRoast;
    if (vendorId == 0x1AF4) {
        gpuRoast = "no GPU, just raw CPU willpower. Get proper GPU drivers!";
        gpuBonus = -50;
    } else if (vendorId == 0x8086) {
        gpuRoast = "a noble choice for spreadsheets.";
        gpuBonus = -1;
    } else if (vendorId == 0x10DE) {
        gpuRoast = "welcome to Team Green.";
        gpuBonus = +5;
    } else if (vendorId == 0x1002) {
        gpuRoast = "Clever choice, you escaped NVIDIA aura-taxes.";
        gpuBonus = +3;
    } else if (vendorId == 0x106B) {
        gpuRoast = "efficient, fast, and impossible to upgrade...";
        gpuBonus = +2;
    } else if (vendorId == 0x13B5) {
        gpuRoast = "mobile silicon doing desktop duty.";
        gpuBonus = 0;
    } else {
        gpuRoast = "either very new or very irrelevant.";
    }
    g_hardwareJudgement.comments.push_back(gpu + " detected: " + gpuRoast);

    int rstBonus = 0;
    switch (Renderer::getRenderSystemType()) {
        case RST_VULKAN:
            g_hardwareJudgement.comments.push_back("Vulkan Renderer used - Nothing to shame here...");
            rstBonus = +5;
            break;
        case RST_DIRECTX11:
            g_hardwareJudgement.comments.push_back("DirectX 11 Renderer used - Older and very, very Windows...");
            rstBonus = -2;
            break;
        case RST_DIRECTX12:
            g_hardwareJudgement.comments.push_back("DirectX 12 Renderer used - Too Microsoft to use Vulkan I guess?");
            rstBonus = +2;
            break;
        case RST_OPENGL2:
        case RST_OPENGL4:
            g_hardwareJudgement.comments.push_back("OpenGL Renderer used - Oh my god, are you kidding me??");
            rstBonus = -20;
            break;
        case RST_OPENGLES:
            g_hardwareJudgement.comments.push_back("OpenGLES Renderer used - Probably Android user...");
            rstBonus = -10;
            break;
        case RST_METAL:
            g_hardwareJudgement.comments.push_back("Metal Renderer used - Guys look! A loyal Apple user!");
            rstBonus = +5;
            break;
        case RST_WEBGL:
            g_hardwareJudgement.comments.push_back("WebGL Renderer used - Ouch... How dare you! Running me in a BROWSER!??");
            rstBonus = +5;
            break;
        default:
            g_hardwareJudgement.comments.push_back("No Renderer detected - Uhm...");
            rstBonus = -100;
    }
    return rstBonus + gpuBonus;
}
#else
malus_or_bonus HardwareJudger::judgeGPU() {
    g_hardwareJudgement.comments.push_back("GPU judgement skipped in headless mode.");
    return 0;
}
#endif

malus_or_bonus HardwareJudger::judgeRAM() {
    long ramSizeInGB = 0;

#if defined(__linux__)
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal") != std::string::npos) {
            std::istringstream iss(line);
            std::string key, unit;
            long memKB;
            iss >> key >> memKB >> unit;
            ramSizeInGB = (memKB + 512 * 1024) / 1024 / 1024;
            break;
        }
    }
#elif defined(__APPLE__)
    int64_t memBytes = 0;
    size_t size = sizeof(memBytes);
    sysctlbyname("hw.memsize", &memBytes, &size, NULL, 0);
    ramSizeInGB = memBytes / 1024 / 1024 / 1024;
#elif defined(_WIN32)
    MEMORYSTATUSEX memoryStatus{};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (!GlobalMemoryStatusEx(&memoryStatus)) {
        return 0;
    }
    ramSizeInGB = static_cast<long>((memoryStatus.ullTotalPhys + 512ULL * 1024 * 1024) / 1024 / 1024 / 1024);
#endif

    if (ramSizeInGB == 0) {
        g_hardwareJudgement.comments.push_back("No RAM detected. Maybe try using a potato.");
        return -100;
    }
    if (ramSizeInGB < 4) {
        g_hardwareJudgement.comments.push_back(std::to_string(ramSizeInGB) + " GB RAM - even Notepad++ is scared.");
        return -20;
    }
    if (ramSizeInGB < 8) {
        g_hardwareJudgement.comments.push_back(std::to_string(ramSizeInGB) + " GB RAM - again, welcome back to 2010!");
        return -5;
    }
    if (ramSizeInGB < 16) {
        g_hardwareJudgement.comments.push_back(std::to_string(ramSizeInGB) + " GB RAM - the minimum for pretending you're modern.");
    } else if (ramSizeInGB < 32) {
        g_hardwareJudgement.comments.push_back(std::to_string(ramSizeInGB) + " GB RAM - glorious multitasking awaits.");
    } else if (ramSizeInGB < 64) {
        g_hardwareJudgement.comments.push_back(std::to_string(ramSizeInGB) + " GB RAM - probably a content creator, or lying.");
    } else {
        g_hardwareJudgement.comments.push_back(std::to_string(ramSizeInGB) + " GB RAM - you either simulate galaxies or just *can*.");
    }
    return ramSizeInGB / 2.0f;
}

bonus HardwareJudger::judgeYourself() {
    #if defined(__linux__)
        g_hardwareJudgement.comments.push_back("You use Linux. Either elite hacker or steam deck user!");
        return +5;
    #elif defined(__APPLE__)
        g_hardwareJudgement.comments.push_back("You're on a Mac. Probably drinking a matcha latte right now.");
        return +3;
    #elif defined(_WIN32)
        g_hardwareJudgement.comments.push_back("You use Windows. Brave.");
        return -2;
    #else
        g_hardwareJudgement.comments.push_back("Your OS is unknown. You're either a time traveler or cursed.");
        return 0;
    #endif
}

malus_or_bonus HardwareJudger::judgeNetwork() {
    std::string address = sm_gameServerAddress;
    std::string command;

    if (address.empty()) {
        g_hardwareJudgement.comments.push_back("No Game Server Address provided! Fortunately for you since your mangy network escapes judgement...");
        return 0;
    }

#if defined(_WIN32)
    command = "ping -n 1 " + address + " > " + PING_TEMP_FILE;
#elif defined(__APPLE__)
    command = "/sbin/ping -c 1 " + address + " > " + PING_TEMP_FILE + " 2>&1";
#else
    command = "ping -c 1 " + address + " > " + PING_TEMP_FILE + " 2>&1";
#endif

    int exitCode = std::system(command.c_str());
    if (exitCode != 0) {
        g_hardwareJudgement.comments.push_back("Ping command failed. Did you plug in the mysterious WiFi Cable? Anyway...");
    }

    std::ifstream file(PING_TEMP_FILE);
    if (!file.is_open()) {
        g_hardwareJudgement.comments.push_back("Ping file missing. Is the filesystem haunted?");
        return 20;
    }

    std::string line;
    std::string result;
    while (std::getline(file, line)) {
        result += line + "\n";
    }
    file.close();
    std::remove(PING_TEMP_FILE.c_str()); // Clean up

    std::smatch match;
    std::regex pingRegex(R"([=<]?([\d.,]+)\s*ms)", std::regex_constants::icase);

    if (std::regex_search(result, match, pingRegex)) {
        float ping = std::stof(match[1]);

        if (ping < 15) {
            g_hardwareJudgement.comments.push_back("Ping: excellent. Your ISP is worthy.");
        } else if (ping < 60) {
            g_hardwareJudgement.comments.push_back("Ping: fine. Nobody dies *too* early.");
        } else if (ping < 150) {
            g_hardwareJudgement.comments.push_back("Ping: worrisome. Prepare for teleportation deaths.");
        } else {
            g_hardwareJudgement.comments.push_back("Ping (10 seconds later...): disaster. The packets arrive via Pigeon. \n\tRecommended upgrade: smoke signals.");
        }

        return ping;
    } else {
        g_hardwareJudgement.comments.push_back("Couldn't determine ping. Did you unplug the server?");
        return 100;
    }
}

bonus HardwareJudger::judgeEnvironment() {
    #if defined(__linux__)
        const char* desktop = getenv("XDG_CURRENT_DESKTOP");
        if (!desktop) desktop = getenv("DESKTOP_SESSION");
        if (!desktop) {
            g_hardwareJudgement.comments.push_back("No desktop environment detected. Hardcore terminal user?");
            return +5;
        }

        std::string env = desktop;
        std::transform(env.begin(), env.end(), env.begin(), ::tolower);

        if (env.find("kde") != std::string::npos) {
            g_hardwareJudgement.comments.push_back("KDE user detected - so many sliders, so little time.");
            return +5;
        }
        if (env.find("gnome") != std::string::npos) {
            g_hardwareJudgement.comments.push_back("GNOME user - minimalism, enforced by devs with strong opinions.");
            return +2;
        }
        if (env.find("xfce") != std::string::npos) {
            g_hardwareJudgement.comments.push_back("XFCE - lightweight and proud. Efficient!");
            return +1;
        }
        if (env.find("lxde") != std::string::npos || env.find("lxqt") != std::string::npos) {
            g_hardwareJudgement.comments.push_back("LXDE/QT - you like your DE like your coffee: instant.");
            return +2;
        }
        if (env.find("budgie") != std::string::npos) {
            g_hardwareJudgement.comments.push_back("Budgie user - sleek and underrated.");
            return +1;
        }
        if (env.find("unity") != std::string::npos) {
            g_hardwareJudgement.comments.push_back("Unity - huh... you're still alive?");
            return +2;
        }
        if (env.find("i3") != std::string::npos || env.find("sway") != std::string::npos) {
            g_hardwareJudgement.comments.push_back("Tiling WM detected - welcome, wizard.");
            return +4;
        }

        g_hardwareJudgement.comments.push_back("Unknown desktop environment - possibly cursed.");
        return +0;

    #else
        //g_hardwareJudgement.comments.push_back("Desktop environment detection not supported on this OS.");
        return 0;
    #endif
}

malus_or_bonus HardwareJudger::judgeAll(bool override, bool noNetwork, bool noGPU) {
    bool withGPU, withNetwork;
    hasJudged(withNetwork, withGPU);
    if (!override && sm_hardwareIsJudged && noNetwork != withNetwork && noGPU != withGPU) return g_hardwareJudgement.finalScore;

    g_hardwareJudgement.comments.clear();
    g_hardwareJudgement.hardwareCategory = HC_EMPTY;
    
    if (sm_hardwareIsJudged) Logger::log(LET_WARN, "Orka, Hardware Judger", "Rejudging device... Don't expect better results this time!");
    else Logger::log(LET_WARN, "Orka, Hardware Judger", "Judging device... Prepare to be shamed! ");

    sm_judgeResultHasNoGPU = noGPU;
    sm_judgeResultHasNoNetwork = noNetwork;

    // The sacred algorithm
    float total = 0;
    total += judgeCPU() * 2;
    total += noGPU ? 0 : judgeGPU() / 4;
    total += judgeRAM() / 1.5;
    total -= noNetwork ? 0 : judgeNetwork() / 3.7;
    total += judgeYourself() / 5;
    total += judgeEnvironment() / 5;

    if (total < -100)
        g_hardwareJudgement.hardwareCategory = HC_CALCULATOR;
    else if (total < -20)
        g_hardwareJudgement.hardwareCategory = HC_EMBEDDED;
    else if (total < 0)
        g_hardwareJudgement.hardwareCategory = HC_POTATO;
    else if (total < 30)
        g_hardwareJudgement.hardwareCategory = HC_AVERAGE;
    else if (total < 80)
        g_hardwareJudgement.hardwareCategory = HC_RICH;
    else if (total < 120)
        g_hardwareJudgement.hardwareCategory = HC_NASA_ADMIN;
    else
        g_hardwareJudgement.hardwareCategory = HC_DATA_CENTER;

    g_hardwareJudgement.comments.push_back("Final hardware score: " + std::to_string(total));
    g_hardwareJudgement.finalScore = total;
    sm_hardwareIsJudged = true;
    return total;
}

void HardwareJudger::printToTerminal() {
    judgeAll();
    for (auto l : g_hardwareJudgement.comments) {
        Logger::log(LET_SPECIAL, "Orka, Hardware Judger", l);
    }
    switch (g_hardwareJudgement.hardwareCategory) {
        case HC_CALCULATOR:
        Logger::log(LET_CRITICAL_ERROR, "Orka, Hardware Judger", "Category: Calculator - Yes, really. Your hardware is doing its best. How did you even end up here??"); break;
        case HC_EMBEDDED:
        Logger::log(LET_ERROR, "Orka, Hardware Judger", "Category: Embedded - Not even meant for this, but here we are."); break;
        case HC_POTATO:
        Logger::log(LET_ERROR, "Orka, Hardware Judger", "Category: Potato - Fries not included!"); break;
        case HC_AVERAGE:
        Logger::log(LET_WARN, "Orka, Hardware Judger", "Category: Average - Congrats, you're forgettable."); break;
        case HC_RICH:
        Logger::log(LET_INFO, "Orka, Hardware Judger", "Category: Rich Boy - You probably don't even know what half this stuff does!"); break;
        case HC_NASA_ADMIN:
        Logger::log(LET_ERROR, "Orka, Hardware Judger", "Category: Nasa Admin - Powerful enough to simulate Mars, used to simulate our physics bugs..."); break;
        case HC_DATA_CENTER:
        Logger::log(LET_BLOCK, "Orka, Hardware Judger", "Category: Data Center - Seriously! Turn off something before you trip a breaker!\n\tEngine should actually refuse to start on this monstrosity! This is for games, not for science! Proceeding anyway..."); break;
        default: break;
    }
    sm_hasPrintedToTerminal = true;
}

