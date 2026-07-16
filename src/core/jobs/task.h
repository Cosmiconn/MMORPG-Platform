#pragma once

#include "core/profiling/tracy_seed.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace seed::jobs {

// ---------------------------------------------------------------------------
// TaskState
// ---------------------------------------------------------------------------
enum class TaskState : uint8_t {
    Pending,    // Created, not ready yet
    Ready,      // All dependencies met
    Running,    // Currently executing
    Completed,  // Done
    Failed      // Error
};

// ---------------------------------------------------------------------------
// Task
// ---------------------------------------------------------------------------
// A unit of work with optional dependencies.
//
// Thread-safety:
//   - state and unfinishedDependencies are atomic.
//   - dependentsMutex protects dependents vector during graph construction.
//   - dependents vector is read-only after submit().
// ---------------------------------------------------------------------------
struct Task {
    using Func = std::function<void()>;

    Func work;
    std::string name;

    std::atomic<TaskState> state{TaskState::Pending};
    std::atomic<uint32_t> unfinishedDependencies{0};
    std::vector<Task*> dependents;
    std::mutex dependentsMutex; // Only used during graph construction

    explicit Task(Func w, const char* n = "unnamed")
        : work(std::move(w)), name(n ? n : "unnamed") {}

    // Called by a worker when this task finishes.
    // Decrements dependency counters and may push ready dependents to the
    // global work queue.
    void onDependencyCompleted(class JobSystem* js);
};

// ---------------------------------------------------------------------------
// TaskHandle
// ---------------------------------------------------------------------------
// Lightweight opaque handle to a Task.  Valid only while the JobSystem
// that created it is alive.
// ---------------------------------------------------------------------------
class TaskHandle {
    Task* m_task = nullptr;
public:
    TaskHandle() = default;
    explicit TaskHandle(Task* t) : m_task(t) {}

    Task* get() const { return m_task; }
    explicit operator bool() const { return m_task != nullptr; }

    bool operator==(const TaskHandle& other) const { return m_task == other.m_task; }
    bool operator!=(const TaskHandle& other) const { return m_task != other.m_task; }
};

} // namespace seed::jobs
