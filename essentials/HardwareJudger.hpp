/*
 * File: HardwareJudger.hpp
 *
 * Please don't take this too seriously:
 * This very unprofessional and heavily biased "judger" classifies your hardware
 * and dishes out passive-aggressive, hardware-shaming feedback
 * 
 * Use cases:
 * - Adjusting default settings (e.g. physics detail, ragdoll limits)
 * - Informative logs / feedback for testers
 * - Forcing potato mode (if really, really necessary)
 * - Evaluating host suitability in P2P games. 
 * 
 * The hardware points are precise and make sense! (as long as you don't have a Mac or a Pentium 1...)!
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <string>
#include <string_view>
#include <vector>

enum HardwareCategory {
    HC_EMPTY = -1,
    HC_CALCULATOR = 0,       // Yes, really. Your hardware is doing its best. AVR? RISC-V?
    HC_EMBEDDED = 1,         // Not even meant for this, but here we are...
    HC_POTATO = 2,           // Fries not included.
    HC_AVERAGE = 3,          // Congrats, you're forgettable.
    HC_RICH = 4,             // You probably don’t even know what half this stuff does!
    HC_NASA_ADMIN = 5,       // Powerful enough to simulate Mars, used to simulate our physics bugs...
    HC_DATA_CENTER = 6       // Seriously! Turn off something before you trip a breaker!
};

struct HardwareOutput {
    HardwareCategory hardwareCategory = HC_EMPTY; // Your hardware category (see HardwareCategory)
    std::vector<std::string> comments; // Passive-aggressive personal judgments
    float finalScore = -100000; // The final hardware score
};

// The sacred output: all judgments end up here. Worship accordingly. (intentionally public)
inline HardwareOutput g_hardwareJudgement;

using malus = float;
using bonus = float;
using malus_or_bonus = float;

class HardwareJudger {
public:
    // Judges your PC and generates scores. Use override to judge again.
    static malus_or_bonus judgeAll(bool override=false, bool noNetwork=false, bool noGPU=false);

    // Logs this to terminal!
    static void printToTerminal();

    // Returns true if the HardwareJudger has already done its job
    inline static bool hasJudged() { return sm_hardwareIsJudged; }
    inline static bool hasJudged(bool& withNetwork, bool& withGPU) { withNetwork = !sm_judgeResultHasNoNetwork; withGPU = !sm_judgeResultHasNoGPU; return sm_hardwareIsJudged; }
    inline static bool hasPrintedToTerminal() { return sm_hasPrintedToTerminal; }

    inline static void setGameServerAddress(std::string_view address) { sm_gameServerAddress = address; }
private:
    // You might redeem yourself here
    static malus_or_bonus judgeCPU();
    // Alright! Time to plug in the mysterious WiFi Cable
    static malus_or_bonus judgeNetwork();
    // Sorry Intel...
    static malus_or_bonus judgeGPU();
    // RAM. Not more.
    static malus_or_bonus judgeRAM();
    // Yep, you – the user. Read the docs? You get points!
    static bonus judgeYourself();
    // Sorry LXDE
    static bonus judgeEnvironment();

    inline static bool sm_hardwareIsJudged, sm_judgeResultHasNoNetwork, sm_judgeResultHasNoGPU, sm_hasPrintedToTerminal;
    inline static std::string sm_gameServerAddress;
};

