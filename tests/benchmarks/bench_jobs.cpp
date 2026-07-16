#include "core/jobs/job_system.h"
#include <chrono>
#include <cstdio>
#include <atomic>
#include <vector>

using namespace seed::jobs;

int main() {
    constexpr size_t N = 1'000'000;

    // Benchmark 1: 1M simple tasks
    {
        JobSystem js({.numWorkers = 8});
        std::atomic<size_t> counter{0};

        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < N; ++i) {
            js.schedule([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
        }
        js.waitForAll();
        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("1M tasks: %.2f ms (%.0f tasks/sec)\n", ms, N / (ms / 1000.0));
        std::printf("Counter: %zu\n", counter.load());
    }

    // Benchmark 2: parallelFor 1M elements
    {
        JobSystem js({.numWorkers = 8});
        std::vector<int> data(N, 0);

        auto t0 = std::chrono::high_resolution_clock::now();
        js.parallelFor(data.size(), [&](size_t i) {
            data[i] = static_cast<int>(i);
        });
        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("parallelFor 1M: %.2f ms\n", ms);
    }

    return 0;
}
