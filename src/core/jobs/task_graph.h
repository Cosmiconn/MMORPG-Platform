#pragma once

#include "core/jobs/task.h"
#include <memory>
#include <vector>

namespace seed::jobs {

class JobSystem; // forward declaration

// ---------------------------------------------------------------------------
// TaskGraph
// ---------------------------------------------------------------------------
// Builder for a directed acyclic graph of tasks.
//
// Usage:
//   TaskGraph graph;
//   auto a = graph.createTask([]{ ... }, "A");
//   auto b = graph.createTask([]{ ... }, "B");
//   graph.addDependency(a, b);   // B depends on A
//   graph.submit(&js);            // enqueue all ready tasks
//
// Thread-safety: NOT thread-safe.  Build the graph from a single thread,
// then submit.
// ---------------------------------------------------------------------------
class TaskGraph {
public:
    TaskGraph() = default;
    ~TaskGraph() = default;

    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;
    TaskGraph(TaskGraph&&) = default;
    TaskGraph& operator=(TaskGraph&&) = default;

    TaskHandle createTask(Task::Func work, const char* name = "unnamed");
    void addDependency(TaskHandle before, TaskHandle after);

    // Submit all root tasks (those with zero dependencies) to the JobSystem.
    // After submit(), the graph must not be modified.
    void submit(JobSystem* js);

    size_t numTasks() const { return m_tasks.size(); }

private:
    std::vector<std::unique_ptr<Task>> m_tasks;
    std::vector<TaskHandle> m_roots;
};

} // namespace seed::jobs
