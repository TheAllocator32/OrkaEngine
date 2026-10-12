/*
 * File: Timing.hpp
 *
 * Contains Scheduler and Timer
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include "Threading.hpp"
#include <functional>
#include <initializer_list>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class Scheduler {
public:
    // Runs the given functions parallel in different threads. Returns a progressID that you may use to track the progress using the ProgressManager
    static int parallelize(std::initializer_list<std::function<void()>> functions);
    static int parallelize(std::vector<std::function<void()>> functions);
    // Runs the given functions parallel in different threads and waits until all threads finished!
    static void parallelizeBlocking(std::initializer_list<std::function<void()>> functions);
    static void parallelizeBlocking(std::vector<std::function<void()>> functions);
    template<typename... Fs>
    inline static void parallelizeBlocking(Fs&&... fs) {
        static_assert((std::is_invocable_r_v<void, std::remove_reference_t<Fs>> && ...), "All arguments must be callable as void()");
        parallelizeBlocking(std::initializer_list<std::function<void()>>{std::function<void()>(std::forward<Fs>(fs))...});
    }
    
    // Adds a lambda to the main loop (render) queue. The worker will run x Lambdas every frame depending on the hardware score. If the priority is 1 it will run x lambdas; If the priority is 2 it will run x/2 lambdas every frame etc. It will run lambdas until the next lambda would exceed the work queue limit
    static void schedule(std::function<void()> function, short priority = 1);
    // Runs a lambda on the main thread and blocks until it completed.
    static void runOnMainThreadBlocking(std::function<void()> function, short priority = 1);
    // Schedule a lambda function. It will be later invoked from a worker thread. Timers are updated every millisecond.
    static void scheduleTime(std::function<void()> function, int time);
    
    // Called internally...
    static void workQueue();
    

private:
    inline static threadsafe<std::queue<std::tuple<int, std::function<void()>>>> sm_queue;
};

class Timing {
public:
    // Returns the time (in seconds) since getDelta was last called with the same ID.
    static float getDelta(std::string_view id);
private:
    inline static threadsafe<std::unordered_map<std::string, std::chrono::steady_clock::time_point>> sm_last; // I really love long chains of <<<>>>
};

