/*
 * File: Threading.hpp
 *
 * Contains Threadsave class and Threading tools - Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "Threading.hpp"

void Threading::launchWorker(std::string_view name) {
    if (name.empty())
        throw std::invalid_argument("Worker name must not be empty");

    auto workerName = std::string(name);
    auto worker = std::make_shared<WorkerRuntime>(workerName);
    auto& workers = registry();

    {
        std::lock_guard<std::mutex> lock(workers.mutex);
        if (workers.workers.contains(workerName))
            return;

        workers.workers.emplace(workerName, worker);
    }

    worker->thread = std::thread([worker]() {
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->id = std::this_thread::get_id();
        }

        workerLoop(worker);
    });
}

void Threading::stopWorker(std::string_view name) {
    auto workerName = std::string(name);
    std::shared_ptr<WorkerRuntime> worker;
    auto& workers = registry();

    {
        std::lock_guard<std::mutex> lock(workers.mutex);
        auto it = workers.workers.find(workerName);
        if (it == workers.workers.end())
            throw std::runtime_error("Worker '" + workerName + "' not found");

        worker = it->second;
        workers.workers.erase(it);
    }

    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->acceptingTasks = false;
        worker->stopping = true;
    }

    worker->condition.notify_one();
    finishWorkerThread(worker);
}

void Threading::stopAllWorkers() {
    std::vector<std::shared_ptr<WorkerRuntime>> workers;
    auto& workerRegistry = registry();

    {
        std::lock_guard<std::mutex> lock(workerRegistry.mutex);
        for (auto& [name, worker] : workerRegistry.workers)
            workers.push_back(worker);
        workerRegistry.workers.clear();
    }

    for (auto& worker : workers) {
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->acceptingTasks = false;
            worker->stopping = true;
        }
        worker->condition.notify_one();
    }

    for (auto& worker : workers) {
        finishWorkerThread(worker);
    }
}

bool Threading::hasWorker(std::string_view name) {
    auto& workers = registry();
    std::lock_guard<std::mutex> lock(workers.mutex);
    return workers.workers.contains(std::string(name));
}

std::vector<Threading::Worker> Threading::listWorkers() {
    std::vector<Worker> result;
    auto& workers = registry();

    std::lock_guard<std::mutex> workersLock(workers.mutex);
    result.reserve(workers.workers.size());

    for (auto& [name, worker] : workers.workers) {
        std::lock_guard<std::mutex> workerLock(worker->mutex);
        result.push_back({ worker->name, worker->id, worker->tasks.size() });
    }

    return result;
}

std::future<void> Threading::addTask(std::string_view workerName, Task task) {
    if (!task)
        throw std::invalid_argument("Task must not be empty");

    auto worker = getWorkerRuntime(workerName);
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (!worker->acceptingTasks)
            throw std::runtime_error("Worker '" + worker->name + "' is stopping");

        worker->tasks.push([task = std::move(task), promise]() mutable {
            try {
                task();
                promise->set_value();
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
    }

    worker->condition.notify_one();
    return future;
}

void Threading::addTaskBlocking(std::string_view workerName, Task task) {
    if (!task)
        throw std::invalid_argument("Task must not be empty");

    auto worker = getWorkerRuntime(workerName);
    bool runInline = false;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        runInline = worker->id == std::this_thread::get_id();
    }

    if (runInline) {
        task();
        return;
    }

    addTask(workerName, std::move(task)).get();
}

std::shared_ptr<Threading::WorkerRuntime> Threading::getWorkerRuntime(std::string_view name) {
    auto workerName = std::string(name);
    auto& workers = registry();
    std::lock_guard<std::mutex> lock(workers.mutex);

    auto it = workers.workers.find(workerName);
    if (it == workers.workers.end())
        throw std::runtime_error("Worker '" + workerName + "' not found");

    return it->second;
}

Threading::WorkerRegistry::~WorkerRegistry() {
    std::vector<std::shared_ptr<WorkerRuntime>> activeWorkers;

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& [name, worker] : workers)
            activeWorkers.push_back(worker);
        workers.clear();
    }

    for (auto& worker : activeWorkers) {
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->acceptingTasks = false;
            worker->stopping = true;
        }
        worker->condition.notify_one();
    }

    for (auto& worker : activeWorkers) {
        finishWorkerThread(worker);
    }
}

Threading::WorkerRegistry& Threading::registry() {
    static WorkerRegistry instance;
    return instance;
}

void Threading::finishWorkerThread(const std::shared_ptr<WorkerRuntime>& worker) {
    if (!worker->thread.joinable())
        return;

    if (worker->thread.get_id() == std::this_thread::get_id()) {
        worker->thread.detach();
        return;
    }

    worker->thread.join();
}

void Threading::workerLoop(const std::shared_ptr<WorkerRuntime>& worker) {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->condition.wait(lock, [&worker]() {
                return worker->stopping || !worker->tasks.empty();
            });

            if (worker->tasks.empty()) {
                if (worker->stopping)
                    return;
                continue;
            }

            task = std::move(worker->tasks.front());
            worker->tasks.pop();
        }

        task();
    }
}

