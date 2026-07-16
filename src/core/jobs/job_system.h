#pragma once

#include "core/jobs/task.h"
#include "core/jobs/work_stealing_queue.h"
#include "core/profiling/tracy_seed.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace seed::jobs {

class TaskGraph;

// ---------------------------------------------------------------------------
// JobSystem
// ---------------------------------------------------------------------------
// Thread-pool with work-stealing queues, task dependencies, and barriers.
//
// Design:
//   - One queue per worker (Chase-Lev).
//   - Main thread may also submit work via worker 0's queue.
//   - Tasks with dependencies are tracked globally; when a task finishes it
//     decrements counters of its dependents and may push them to queues.
//   - waitForAll() participates in work-stealing to avoid deadlocks.
//
// Thread-safety:
//   - schedule(), createTask(), addDependency(), submit(), waitForAll(),
//     parallelFor(), barrier() are thread-safe.
// ---------------------------------------------------------------------------
class JobSystem {
public:
    struct Config {
        uint32_t numWorkers = std::thread::hardware_concurrency();
    };

    explicit JobSystem(const Config& cfg = {});
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem(JobSystem&&) = delete;
    JobSystem& operator=(JobSystem&&) = delete;

    // -----------------------------------------------------------------------
    // Simple task submission
    // -----------------------------------------------------------------------
    void schedule(std::function<void()> work, const char* name = "unnamed");

    // -----------------------------------------------------------------------
    // DAG tasks
    // -----------------------------------------------------------------------
    TaskHandle createTask(std::function<void()> work, const char* name = "unnamed");
    void addDependency(TaskHandle before, TaskHandle after);
    void submit(TaskHandle task);

    // -----------------------------------------------------------------------
    // Synchronisation
    // -----------------------------------------------------------------------
    void waitForAll();
    void barrier();

    // -----------------------------------------------------------------------
    // Parallel for
    // -----------------------------------------------------------------------
    template<typename Func>
    void parallelFor(size_t count, Func&& func);

    // -----------------------------------------------------------------------
    // Diagnostics
    // -----------------------------------------------------------------------
    uint32_t numWorkers() const { return static_cast<uint32_t>(m_workers.size()); }
    uint32_t activeTasks() const { return m_activeTasks.load(std::memory_order_relaxed); }

private:
    struct Worker {
        WorkStealingQueue queue;
        std::thread thread;
        uint32_t id = 0;
    };

    std::vector<std::unique_ptr<Worker>> m_workers;
    std::atomic<bool> m_shutdown{false};

    // Global task tracking for dependency resolution
    std::mutex m_taskMutex;
    std::vector<std::unique_ptr<Task>> m_allTasks;
    std::atomic<uint32_t> m_activeTasks{0};

    // Synchronisation for waitForAll / barrier
    std::mutex m_waitMutex;
    std::condition_variable m_waitCv;
    std::atomic<uint32_t> m_waitingWorkers{0};

    // Internal helpers
    void workerLoop(uint32_t workerId);
    void executeTask(Task* task);
    void pushToWorker(Task* task);
    Task* stealFromOther(uint32_t thiefId);
    bool helpOut(uint32_t workerId); // returns true if work was found
    void waitUntilIdle();
};

// ---------------------------------------------------------------------------
// parallelFor implementation
// ---------------------------------------------------------------------------
template<typename Func>
void JobSystem::parallelFor(size_t count, Func&& func) {
    SEED_ZONE("JobSystem::parallelFor");
    if (count == 0) return;

    const uint32_t workers = numWorkers();
    if (workers == 0 || count == 1) {
        for (size_t i = 0; i < count; ++i) func(i);
        return;
    }

    std::atomic<size_t> nextIndex{0};
    const size_t numChunks = std::min<size_t>(count, static_cast<size_t>(workers) * 4);
    std::atomic<size_t> completedChunks{0};

    for (size_t c = 0; c < numChunks; ++c) {
        schedule([&nextIndex, count, &func, &completedChunks]() {
            SEED_ZONE("parallelFor chunk");
            while (true) {
                size_t i = nextIndex.fetch_add(1, std::memory_order_relaxed);
                if (i >= count) break;
                func(i);
            }
            completedChunks.fetch_add(1, std::memory_order_release);
        }, "parallelFor_chunk");
    }

    // Help out until all chunks are done
    while (completedChunks.load(std::memory_order_acquire) < numChunks) {
        helpOut(0);
        if (completedChunks.load(std::memory_order_acquire) >= numChunks) break;
        std::this_thread::yield();
    }
}

} // namespace seed::jobs
