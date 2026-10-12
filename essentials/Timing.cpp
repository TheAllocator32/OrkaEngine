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

#include "HardwareJudger.hpp"
#include "Logger.hpp"
#include "Threading.hpp"
#include "Timing.hpp"
#include <exception>
#include <functional>
#include <initializer_list>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

void Scheduler::schedule(std::function<void()> function, short priority) {
    auto queue = sm_queue.get();
    queue->emplace(priority, std::move(function));
}

void Scheduler::runOnMainThreadBlocking(std::function<void()> function, short priority) {
    if (!function) {
        return;
    }
    if (Threading::amIMainThread()) {
        function();
        return;
    }

    struct BlockingTaskState {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
    };

    auto state = std::make_shared<BlockingTaskState>();
    schedule([state, function = std::move(function)]() mutable {
        try {
            function();
        }
        catch (const std::exception& e) {
            Logger::log(LET_WARN, "Orka, Scheduler", e.what());
        }
        catch (...) {
            Logger::log(LET_WARN, "Orka, Scheduler", "Unknown exception while executing main-thread task.");
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->done = true;
        }
        state->cv.notify_one();
    }, priority);

    std::unique_lock<std::mutex> lock(state->mutex);
    state->cv.wait(lock, [&state]() { return state->done; });
}

int Scheduler::parallelize(std::initializer_list<std::function<void()>> functions) {
    if (functions.size() == 0) {
        return -1;
    }
    for (auto f : functions) {
        std::thread([f]() {
            try {
                f();
            }
            catch (const std::exception& e) {
                Logger::log(LET_WARN, "Orka, Scheduler", e.what());
            }
            catch (...) {
                Logger::log(LET_WARN, "Orka, Scheduler", "Unknown exception while executing task (very helpful message, I know)");
            }
            }).detach();
    }
    return -1;
}

int Scheduler::parallelize(std::vector<std::function<void()>> functions) {
    if (functions.size() == 0) {
        return -1;
    }
    for (auto f : functions) {
        std::thread([f]() {
            try {
                f();
            }
            catch (const std::exception& e) {
                Logger::log(LET_WARN, "Orka, Scheduler", e.what());
            }
            catch (...) {
                Logger::log(LET_WARN, "Orka, Scheduler", "Unknown exception while executing task (very helpful message, I know)");
            }
            }).detach();
    }
    return -1;
}

void Scheduler::parallelizeBlocking(std::initializer_list<std::function<void()>> functions) {
    if (functions.size() == 0) {
        return;
    }
    if (functions.size() == 1) {
        try {
            (*functions.begin())();
        }
        catch (const std::exception& e) {
            Logger::log(LET_WARN, "Orka, Scheduler (blocking)", e.what());
        }
        catch (...) {
            Logger::log(LET_WARN, "Orka, Scheduler (blocking)", "unknown exception in task (Sorry, we don't know either!)");
        }
        return;
    }
    const auto n = functions.size();
    std::vector<std::jthread> threads;
    threads.reserve(n);

    for (auto f : functions) {
        threads.emplace_back([f](std::stop_token) {
            try {
                f();
            }
            catch (const std::exception& e) {
                Logger::log(LET_WARN, "Orka, Scheduler (blocking)", e.what());
            }
            catch (...) {
                Logger::log(LET_WARN, "Orka, Scheduler (blocking)", "unknown exception in task (Sorry, we don't know either!)");
            }
            });
    }
}

void Scheduler::parallelizeBlocking(std::vector<std::function<void()>> functions) {
    if (functions.size() == 0) {
        return;
    }
    if (functions.size() == 1) {
        try {
            functions[0]();
        }
        catch (const std::exception& e) {
            Logger::log(LET_WARN, "Orka, Scheduler (blocking)", e.what());
        }
        catch (...) {
            Logger::log(LET_WARN, "Orka, Scheduler (blocking)", "unknown exception in task (Sorry, we don't know either!)");
        }
        return;
    }
    const auto n = functions.size();
    std::vector<std::jthread> threads;
    threads.reserve(n);

    for (auto f : functions) {
        threads.emplace_back([f](std::stop_token) {
            try {
                f();
            }
            catch (const std::exception& e) {
                Logger::log(LET_WARN, "Orka, Scheduler (blocking)", e.what());
            }
            catch (...) {
                Logger::log(LET_WARN, "Orka, Scheduler (blocking)", "unknown exception in task (Sorry, we don't know either!)");
            }
            });
    }
}

void Scheduler::scheduleTime(std::function<void()> function, int time) {
    std::thread ([time,function]() {
        std::chrono::milliseconds durationTime(time);
        std::this_thread::sleep_for(durationTime);
        function();
        }).detach();
}

void Scheduler::workQueue() {
    if (!Threading::amIMainThread()) {
        Logger::log(LET_WARN, "Orka, Scheduler", "You tried to run workQueue outside the main thread! WorkQueue is called automatically by Orka! Don't call it manually!");
        return;
    }

    int usedPoints = 0;
    const int maxMainQueueLength =
        (g_hardwareJudgement.finalScore <= 20)
        ? 2
        : (g_hardwareJudgement.finalScore / 10);

    while (usedPoints < maxMainQueueLength) {
        std::function<void()> task;
        int priority = 0;

        {
            auto queue = sm_queue.get();
            if (queue->empty()) {
                break;
            }

            auto [queuedPriority, queuedTask] = queue->front();
            if (usedPoints + queuedPriority > maxMainQueueLength) {
                break;
            }

            priority = queuedPriority;
            task = std::move(queuedTask);
            queue->pop();
        }

        task();
        usedPoints += priority;
    }
}

float Timing::getDelta(std::string_view id) {
    auto now = std::chrono::steady_clock::now();

    auto lastMapTS = sm_last.get();
    auto& lastMap = *lastMapTS;
    const std::string idString(id);

    auto it = lastMap.find(idString);
    if (it == lastMap.end()) {
        lastMap[idString] = now;
        return 0.0f;
    }

    float dt = std::chrono::duration<float>(now - it->second).count();
    it->second = now;
    return dt;
}

