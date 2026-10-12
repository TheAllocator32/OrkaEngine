/*
 * File: Threading.hpp
 *
 * Contains threadsafe class and threading tools. To be serious I'm pretty proud of this M)
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#pragma once
#include <mutex>
#include <optional>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <memory>
#include <cassert>
#include <condition_variable>
#include <future>
#include <queue>
#include <stdexcept>

#define DEPRECATED_NOTICE_THREADSAFE [[deprecated("UNSAFE: You have to manually lock/unlock the guard! Read the manual at the beginning of this file. Then read it again.")]]

 /*
  * threadsafe<T> usage overview
  *
  * -----------------------------------------
  * Option 1 (recommended / RAII)
  * -----------------------------------------
  *  - g_vector.get()
  *  - One lock, full control
  *  - Use this for anything non-trivial
  *
  * Example:
  *  threadsafe<vector3df> g_vector;
  *
  *  void modifyVector() {
  *      auto guard = g_vector.get();     // acquires mutex (RAII)
  *      guard->x = 0;
  *      guard->y = 1;
  *      guard->z = 2;
  *
  *      // OR (same thing, just uglier):
  *      vector3df& v = *guard;
  *      v.x = 0;
  *  } // mutex is released here
  *
  * NOTE:
  *  - The guard MUST be stored by value!!
  *  - NEVER write: vector3df& v = *g_vector.get();
  *
  *
  * -----------------------------------------
  * Option 2 (convenience / single command)
  * -----------------------------------------
  *  - g_vector->x = 0;
  *  - Locks per expression and unlocks directly afterwards
  *  - Fine for one-off commands, slow in loops or rows.
  *
  * Example:
  *  void modifyVectorSingle() {
  *      g_vector->x = 0;   // locks & unlocks for THIS expression only
  *  }
  * Avoid:
  * void modifyVectorStupid() {
  *      g_vector->x = 0;
  *      g_vector->y = 1;
  *      g_vector->z = 2;
  *      g_vector->a = 42;
  *      //...
  * }
  *
  *
  * -----------------------------------------
  * Option 3 (low-level / UNSAFE!!!)
  * -----------------------------------------
  *  - lock() / unlock()
  *  - No RAII
  *  - Extremely easy to mess up
  *  - Here be dragons ;)
  *
  * Example:
  *  void modifyVectorBUTTHISISGODDAMNUNSAFE() {
  *      auto& v = g_vector.lock();   // blocks until available
  *      v.x = 0;
  *      g_vector.unlock();           // IMPORTANT!!!!!!
  *  }
  *
  * If you break things using Option 3, that's on you... That should be clear
  *
  *
  * -----------------------------------------
  * Additional notes
  * -----------------------------------------
  *  - threadsafe_internal can be manually unlocked via .unlock()
  *    (object obviously becomes invalid afterwards).
  *
  *  - Prefer threadsafe<std::shared_ptr<T>> (threadsafe_ptr)
  *    for complex objects and shared ownership.
  *
  *  - NEVER keep references or pointers obtained from temporaries:
  *    auto& v = *threadsafeBla.get(); // Guard is temporary; lock released immediately!
  * 
  * 
  *  I strongly recommend reading this twice or even 3 times because I hate RTFM issues!
  */

template<typename T>
// Fully inline class
class threadsafe {
private: // Private before public because else, the compiler complains about the missing threadsafe_internal!
    mutable std::mutex m_mutex;
    T m_object{};

    struct threadsafe_internal {
        std::unique_lock<std::mutex> m_lock;
        T* m_ptr;
        inline threadsafe_internal(std::mutex& m, T& v) : m_lock(m), m_ptr(&v) {}
        inline threadsafe_internal(std::unique_lock<std::mutex>&& lock, T& v) : m_lock(std::move(lock)), m_ptr(&v) {}
        threadsafe_internal(const threadsafe_internal&) = delete;
        threadsafe_internal& operator=(const threadsafe_internal&) = delete;
        inline void unlock() { if (m_lock.owns_lock()) m_lock.unlock(), m_ptr = nullptr; }
        inline T* operator->() { assert(m_ptr && "threadsafe_internal used after unlock! GRAAAHAHAH"); return m_ptr; }
        inline T& operator*() { assert(m_ptr && "threadsafe_internal used after unlock! GRAAAHAHAH"); return *m_ptr; }
        inline T* get() { return m_ptr; }
        inline bool operator!() const { return m_ptr == nullptr; }
        explicit operator bool() const { return m_ptr != nullptr; }
    };
public:
    // Constructor
    threadsafe() = default;
    explicit inline threadsafe(T object) : m_object(std::move(object)) {}
    threadsafe(const threadsafe&) = delete;
    threadsafe& operator=(const threadsafe&) = delete;
    threadsafe(threadsafe&&) = delete;
    threadsafe& operator=(threadsafe&&) = delete;

    // Returns the bare object (NOT THREAD SAVE!!)
    inline const T& getNonThreadsafeButThisIsFuckingDangerousPleaseThinkBeforeYouUseThat() const { return m_object; }
    inline T& getNonThreadsafeButThisIsFuckingDangerousPleaseThinkBeforeYouUseThat() { return m_object; }

    // Lets you use the stored object threadsafe!
    inline threadsafe_internal operator->() { return threadsafe_internal(m_mutex, m_object); }
    //inline threadsafe_internal operator->() const { return threadsafe_internal(m_mutex, const_cast<T&>(m_object)); }

    // Returns threadsafe_internal (on which you can call .get)
    [[nodiscard]] inline threadsafe_internal get() { return threadsafe_internal(m_mutex, m_object); }
    //[[nodiscard]] inline threadsafe_internal get() const { return threadsafe_internal(m_mutex, const_cast<T&>(m_object)); }

    // = new value() operator
    threadsafe& operator=(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_object = std::move(value);
        return *this;
    }

    // Returns threadsafe_internal only when it's currently available!
    std::optional<threadsafe_internal> tryGet() { 
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock); 
        if (lock.owns_lock()) { 
            return threadsafe_internal(std::move(lock), m_object); 
        } 
        return std::nullopt; 
    }

    // Locks the object and returns it (blocks until available) -- USE EXTREMELY CAREFULLY!!!
    DEPRECATED_NOTICE_THREADSAFE [[nodiscard]]inline T& lock() { m_mutex.lock(); return m_object; }

    // Locks if available -- USE EXTREMELY CAREFULLY!!!
    DEPRECATED_NOTICE_THREADSAFE [[nodiscard]] inline std::optional<std::reference_wrapper<T>> tryLock() {
        if (m_mutex.try_lock()) 
            return std::ref(m_object); 
        else 
            return std::nullopt; 
    }

    // Unlocks the object after you locked it manually via lock() -- USE EXTREMELY CAREFULLY!!! -- NEVER FORGET TO CALL THIS AFTER YOU called lock() or successful tryLock()!
    DEPRECATED_NOTICE_THREADSAFE  inline void unlock() { m_mutex.unlock(); }
};

class Threading {
public:
    using Task = std::function<void()>;

    struct Worker {
        std::string name;
        std::thread::id id;
        std::size_t pendingTasks;
    };

    // Starts a persistent named worker thread.
    static void launchWorker(std::string_view name);
    // Stops a worker after all queued tasks have finished.
    static void stopWorker(std::string_view name);
    // Stops every worker after all queued tasks have finished.
    static void stopAllWorkers();

    static bool hasWorker(std::string_view name);
    static std::vector<Worker> listWorkers();

    // Queues a task and returns immediately. Keep the future if you want to wait for completion or catch task exceptions.
    static std::future<void> addTask(std::string_view workerName, Task task);
    // Queues a task and blocks until it finished. Exceptions thrown by the task are rethrown here.
    static void addTaskBlocking(std::string_view workerName, Task task);

    // Helpers
    // Returns the main thread ID
    inline static const std::thread::id& getMainThreadID() { static const std::thread::id id = std::this_thread::get_id(); return id; }
    // Returns whether the current thread is the main thread
    inline static bool amIMainThread() { return std::this_thread::get_id() == getMainThreadID(); }

private:
    struct WorkerRuntime {
        explicit WorkerRuntime(std::string workerName) : name(std::move(workerName)) {}

        std::string name;
        std::thread thread;
        std::thread::id id;
        std::mutex mutex;
        std::condition_variable condition;
        std::queue<Task> tasks;
        bool acceptingTasks = true;
        bool stopping = false;
    };

    struct WorkerRegistry {
        ~WorkerRegistry();

        std::mutex mutex;
        std::unordered_map<std::string, std::shared_ptr<WorkerRuntime>> workers;
    };

    static WorkerRegistry& registry();
    static std::shared_ptr<WorkerRuntime> getWorkerRuntime(std::string_view name);
    static void finishWorkerThread(const std::shared_ptr<WorkerRuntime>& worker);
    static void workerLoop(const std::shared_ptr<WorkerRuntime>& worker);
};

template<class T>
using threadsafe_ptr = threadsafe<std::shared_ptr<T>>;

#undef DEPRECATED_NOTICE_THREADSAFE

