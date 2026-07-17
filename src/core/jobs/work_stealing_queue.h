#pragma once

#include "core/jobs/task.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace seed::jobs {

// ---------------------------------------------------------------------------
// WorkStealingQueue (Chase-Lev variant)
// ---------------------------------------------------------------------------
// Lock-free single-producer / multi-consumer circular buffer.
//
//   - push() / pop() : owner thread only
//   - steal()        : any thread
//
// Capacity must be a power of two.  We use 256 which is sufficient for
// burst submission without resizing complexity.
// ---------------------------------------------------------------------------
class WorkStealingQueue {
public:
    static constexpr size_t CAPACITY = 65536;
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be power of 2");

    WorkStealingQueue() {
        m_buffer.fill(nullptr);
    }

    // Owner thread only
    void push(Task* task) {
        SEED_ASSERT(task != nullptr, "pushing null task");
        auto b = m_bottom.load(std::memory_order_relaxed);
        m_buffer[b & (CAPACITY - 1)] = task;
        m_bottom.store(b + 1, std::memory_order_release);
    }

    // Owner thread only
    Task* pop() {
        auto b = m_bottom.load(std::memory_order_relaxed) - 1;
        m_bottom.store(b, std::memory_order_relaxed);
        auto t = m_top.load(std::memory_order_acquire);
        if (t <= b) {
            Task* task = m_buffer[b & (CAPACITY - 1)];
            if (t == b) {
                // Last item – race with stealers
                if (!m_top.compare_exchange_strong(t, t + 1,
                        std::memory_order_seq_cst,
                        std::memory_order_relaxed)) {
                    m_bottom.store(b + 1, std::memory_order_relaxed);
                    return nullptr;
                }
                m_bottom.store(b + 1, std::memory_order_relaxed);
            }
            return task;
        } else {
            m_bottom.store(b + 1, std::memory_order_relaxed);
            return nullptr;
        }
    }

    // Any thread
    Task* steal() {
        auto t = m_top.load(std::memory_order_acquire);
        auto b = m_bottom.load(std::memory_order_acquire);
        if (t < b) {
            Task* task = m_buffer[t & (CAPACITY - 1)];
            if (m_top.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst,
                    std::memory_order_relaxed)) {
                return task;
            }
        }
        return nullptr;
    }

    // Approximate size (not synchronised, for diagnostics only)
    size_t size() const {
        auto b = m_bottom.load(std::memory_order_relaxed);
        auto t = m_top.load(std::memory_order_relaxed);
        return (b > t) ? (b - t) : 0;
    }

private:
    std::array<Task*, CAPACITY> m_buffer;

    alignas(64) std::atomic<size_t> m_top{0};    // Steal from here
    alignas(64) std::atomic<size_t> m_bottom{0};  // Push / pop here
};

} // namespace seed::jobs

#ifdef _MSC_VER
#pragma warning(pop)
#endif
