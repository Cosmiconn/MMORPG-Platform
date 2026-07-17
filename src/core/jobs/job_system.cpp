#include "core/jobs/job_system.h"
#include "core/jobs/task_graph.h"
#include <algorithm>
#include <chrono>

namespace seed::jobs {

// ---------------------------------------------------------------------------
// Task::onDependencyCompleted
// ---------------------------------------------------------------------------
void Task::onDependencyCompleted(JobSystem* js) {
    for (Task* dep : dependents) {
        uint32_t remaining = dep->unfinishedDependencies.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0) {
            TaskState expected = TaskState::Pending;
            if (dep->state.compare_exchange_strong(expected, TaskState::Ready,
                    std::memory_order_release, std::memory_order_relaxed)) {
                js->submit(TaskHandle(dep));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// JobSystem
// ---------------------------------------------------------------------------
JobSystem::JobSystem() : JobSystem(Config{}) {}

JobSystem::JobSystem(const Config& cfg) {
    SEED_ZONE("JobSystem::ctor");
    uint32_t n = cfg.numWorkers == 0 ? 1 : cfg.numWorkers;
    m_workers.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        auto w = std::make_unique<Worker>();
        w->id = i;
        m_workers.push_back(std::move(w));
    }
    for (uint32_t i = 0; i < n; ++i) {
        m_workers[i]->thread = std::thread([this, i]() { workerLoop(i); });
    }
}

JobSystem::~JobSystem() {
    SEED_ZONE("JobSystem::dtor");
    waitForAll();
    m_shutdown.store(true, std::memory_order_release);
    m_waitCv.notify_all();
    for (auto& w : m_workers) {
        if (w->thread.joinable()) {
            w->thread.join();
        }
    }
}

void JobSystem::schedule(std::function<void()> work, const char* name) {
    SEED_ZONE("JobSystem::schedule");
    auto task = std::make_unique<Task>(std::move(work), name);
    task->state.store(TaskState::Ready, std::memory_order_relaxed);
    task->unfinishedDependencies.store(0, std::memory_order_relaxed);

    Task* raw = task.get();
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_allTasks.push_back(std::move(task));
    }
    m_activeTasks.fetch_add(1, std::memory_order_relaxed);
    pushToWorker(raw);
}

TaskHandle JobSystem::createTask(std::function<void()> work, const char* name) {
    SEED_ZONE("JobSystem::createTask");
    auto task = std::make_unique<Task>(std::move(work), name);
    Task* raw = task.get();
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_allTasks.push_back(std::move(task));
    }
    return TaskHandle(raw);
}

void JobSystem::addDependency(TaskHandle before, TaskHandle after) {
    SEED_ASSERT(before.get() != nullptr && after.get() != nullptr, "Invalid task handles");
    Task* b = before.get();
    Task* a = after.get();

    {
        std::lock_guard<std::mutex> lock(b->dependentsMutex);
        b->dependents.push_back(a);
    }
    a->unfinishedDependencies.fetch_add(1, std::memory_order_relaxed);
}

void JobSystem::submit(TaskHandle task) {
    SEED_ASSERT(task.get() != nullptr, "submitting null task");
    Task* t = task.get();
    // Guard: a task with remaining dependencies must not be enqueued yet.
    if (t->unfinishedDependencies.load(std::memory_order_acquire) > 0) {
        return;
    }
    TaskState expected = TaskState::Pending;
    if (t->state.compare_exchange_strong(expected, TaskState::Ready,
            std::memory_order_release, std::memory_order_relaxed)) {
        m_activeTasks.fetch_add(1, std::memory_order_relaxed);
        pushToWorker(t);
    }
}

void JobSystem::waitForAll() {
    SEED_ZONE("JobSystem::waitForAll");
    waitUntilIdle();
}

void JobSystem::barrier() {
    SEED_ZONE("JobSystem::barrier");
    waitForAll();
}

void JobSystem::workerLoop(uint32_t workerId) {
    Worker& self = *m_workers[workerId];
    while (!m_shutdown.load(std::memory_order_acquire)) {
        // 1. Try local queue
        Task* task = self.queue.pop();
        if (task) {
            executeTask(task);
            continue;
        }

        // 2. Try global overflow queue
        task = tryGlobalQueue();
        if (task) {
            executeTask(task);
            continue;
        }

        // 3. Try stealing
        bool stole = false;
        for (uint32_t i = 1; i < static_cast<uint32_t>(m_workers.size()); ++i) {
            uint32_t victimId = static_cast<uint32_t>((workerId + i) % m_workers.size());
            if (victimId == workerId) continue;
            task = m_workers[victimId]->queue.steal();
            if (task) {
                executeTask(task);
                stole = true;
                break;
            }
        }
        if (stole) continue;

        // 4. Park if truly idle
        if (m_activeTasks.load(std::memory_order_relaxed) == 0) {
            std::unique_lock<std::mutex> lk(m_waitMutex);
            m_waitingWorkers.fetch_add(1, std::memory_order_relaxed);
            m_waitCv.wait_for(lk, std::chrono::milliseconds(1));
            m_waitingWorkers.fetch_sub(1, std::memory_order_relaxed);
        } else {
            std::this_thread::yield();
        }
    }
}

void JobSystem::executeTask(Task* task) {
    SEED_ZONE(task->name.c_str());
    task->state.store(TaskState::Running, std::memory_order_relaxed);
    task->work();
    task->state.store(TaskState::Completed, std::memory_order_release);

    // Notify dependents
    task->onDependencyCompleted(this);

    // Decrement active counter
    m_activeTasks.fetch_sub(1, std::memory_order_acq_rel);
    m_waitCv.notify_all();
}

void JobSystem::pushToWorker(Task* task) {
    static std::atomic<uint32_t> s_nextWorker{0};
    uint32_t idx = static_cast<uint32_t>(s_nextWorker.fetch_add(1, std::memory_order_relaxed)
                                          % static_cast<uint32_t>(m_workers.size()));
    auto& queue = m_workers[idx]->queue;
    // Chase-Lev queue: push is owner-only and capacity is fixed.
    // If the queue is near capacity, fall back to the global overflow queue.
    if (queue.size() < WorkStealingQueue::CAPACITY - 1) {
        queue.push(task);
    } else {
        std::lock_guard<std::mutex> lock(m_overflowMutex);
        m_overflowQueue.push_back(task);
    }
}

Task* JobSystem::tryGlobalQueue() {
    std::lock_guard<std::mutex> lock(m_overflowMutex);
    if (!m_overflowQueue.empty()) {
        Task* task = m_overflowQueue.back();
        m_overflowQueue.pop_back();
        return task;
    }
    return nullptr;
}

Task* JobSystem::stealFromOther(uint32_t thiefId) {
    for (uint32_t i = 1; i < static_cast<uint32_t>(m_workers.size()); ++i) {
        uint32_t victimId = static_cast<uint32_t>((thiefId + i) % m_workers.size());
        if (victimId == thiefId) continue;
        Task* task = m_workers[victimId]->queue.steal();
        if (task) return task;
    }
    return nullptr;
}

bool JobSystem::helpOut(uint32_t /*workerId*/) {
    // Non-owner threads (including main thread during waitForAll / parallelFor)
    // must ONLY steal or consume from global queue, never pop a worker's local queue.
    Task* task = tryGlobalQueue();
    if (!task) {
        task = stealFromOther(0);
    }
    if (task) {
        executeTask(task);
        return true;
    }
    return false;
}

void JobSystem::waitUntilIdle() {
    while (m_activeTasks.load(std::memory_order_acquire) > 0) {
        helpOut(0);
        if (m_activeTasks.load(std::memory_order_acquire) == 0) break;
        std::this_thread::yield();
    }
}

} // namespace seed::jobs
