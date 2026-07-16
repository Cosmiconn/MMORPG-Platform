#include "core/jobs/task_graph.h"
#include "core/jobs/job_system.h"
#include <algorithm>

namespace seed::jobs {

TaskHandle TaskGraph::createTask(Task::Func work, const char* name) {
    auto task = std::make_unique<Task>(std::move(work), name);
    Task* raw = task.get();
    m_tasks.push_back(std::move(task));
    m_roots.push_back(TaskHandle(raw));
    return TaskHandle(raw);
}

void TaskGraph::addDependency(TaskHandle before, TaskHandle after) {
    SEED_ASSERT(before.get() != nullptr && after.get() != nullptr, "Invalid task handles");
    Task* b = before.get();
    Task* a = after.get();

    {
        std::lock_guard<std::mutex> lock(b->dependentsMutex);
        b->dependents.push_back(a);
    }
    a->unfinishedDependencies.fetch_add(1, std::memory_order_relaxed);

    // Remove 'after' from roots since it now has a dependency
    auto it = std::find(m_roots.begin(), m_roots.end(), after);
    if (it != m_roots.end()) {
        m_roots.erase(it);
    }
}

void TaskGraph::submit(JobSystem* js) {
    for (auto& root : m_roots) {
        js->submit(root);
    }
}

} // namespace seed::jobs
