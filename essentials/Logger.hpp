/*
 * File: Logger.hpp
 *
 * This File contains a basic Logger that logs output in different colors
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <iostream>
#include <string>

// Terminal colors
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

// Log Error Types
enum LOG_ERROR_TYPE {
    LET_INFO,
    LET_WARN,
    LET_BLOCK,
    LET_ERROR,
    LET_CRITICAL_ERROR,
    LET_SPECIAL
};

// Logger Class
class Logger {
public:
    // Logs something to the terminal! Select the Log Level, caller (The name of the system that is logging here) and after that the actual message text
    inline static void log(LOG_ERROR_TYPE type, std::string_view caller, std::string_view message, bool newline=true, bool showInGUI = false) {
        // Keep normal log entries from being appended to an in-place progress line.
        if (sm_progressLineActive) {
            std::cout << "\n" << std::flush;
            sm_progressLineActive = false;
        }
        if (sm_hasLastEntry &&
            type == sm_lastType &&
            caller == sm_lastCaller &&
            message == sm_lastMessage &&
            newline == sm_lastNewline &&
            showInGUI == sm_lastShowInGUI) {
            return;
        }
        sm_lastType = type;
        sm_lastCaller = caller;
        sm_lastMessage = message;
        sm_lastNewline = newline;
        sm_lastShowInGUI = showInGUI;
        sm_hasLastEntry = true;

        std::string prefix;
        std::string color;

        switch (type) {
        case LET_INFO:
            prefix = "[INFO, ";
            color = GREEN;
            break;
        case LET_WARN:
            prefix = "[WARNING, ";
            color = YELLOW;
            break;
        case LET_BLOCK:
            prefix = "[BLOCKED!, ";
            color = BLUE;
            break;
        case LET_ERROR:
            prefix = "[ERROR, ";
            color = RED;
            break;
        case LET_CRITICAL_ERROR:
            prefix = "[CRITICAL ERROR, ";
            color = MAGENTA;
            break;
        case LET_SPECIAL:
            prefix = "[INFO, ";
            color = CYAN;
            break;
        default:
            prefix = "[UNKNOWN, ";
            color = RESET;
            break;
        }

        std::cout << color << prefix << caller << "]" << ": " << message << RESET << (newline ? "\n" : "") << std::flush;

        // TODO: Implement GUI output
    }
    // Starts a terminal line that can be replaced by updateProgressLine().
    inline static void beginProgressLine(std::string_view caller, std::string_view message) {
        finishProgressLine();
        sm_progressCaller = caller;
        sm_progressLineActive = true;
        writeProgressLine(message);
    }
    // Replaces the active progress line. If a regular log temporarily closed it,
    // this creates a new live line with the same caller.
    inline static void updateProgressLine(std::string_view message) {
        if (sm_progressCaller.empty()) return;
        if (sm_progressLineActive) resetLine();
        sm_progressLineActive = true;
        writeProgressLine(message);
    }
    // Commits the current progress line to the terminal.
    inline static void finishProgressLine(std::string_view message = {}) {
        if (sm_progressCaller.empty()) return;
        if (!message.empty()) updateProgressLine(message);
        if (sm_progressLineActive) std::cout << "\n" << std::flush;
        sm_progressLineActive = false;
        sm_progressCaller.clear();
    }
    // Clears lines in terminal! Can be used to unspam the terminal while logging progress.
    inline static void resetLines(int lines) {
        for (int i = 1; i < lines; ++i) {
            std::cout << "\033[F\033[K";
        }
        std::cout << "\r\033[K" << std::flush; 
    }
    // Clears the current line in terminal
    inline static void resetLine() { resetLines(1); }

private:
    inline static void writeProgressLine(std::string_view message) {
        std::cout << "\033[32m[INFO, " << sm_progressCaller << "]: " << message << "\033[0m" << std::flush;
    }
    inline static LOG_ERROR_TYPE sm_lastType = LET_INFO;
    inline static std::string sm_lastCaller;
    inline static std::string sm_lastMessage;
    inline static bool sm_lastNewline = true;
    inline static bool sm_lastShowInGUI = false;
    inline static bool sm_hasLastEntry = false;
    inline static std::string sm_progressCaller;
    inline static bool sm_progressLineActive = false;
};

#undef RESET
#undef RED
#undef GREEN
#undef YELLOW
#undef BLUE
#undef MAGENTA
#undef CYAN

