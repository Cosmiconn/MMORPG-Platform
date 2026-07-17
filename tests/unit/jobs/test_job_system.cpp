#include <doctest/doctest.h>
#include "core/jobs/job_system.h"
#include "core/jobs/task_graph.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace seed::jobs;

TEST_CASE("JobSystem initialization") {
    JobSystem js({.numWorkers = 4});
    REQUIRE(js.numWorkers() == 4);
    REQUIRE(js.activeTasks() == 0);
}

TEST_CASE("JobSystem schedule and execute") {
    JobSystem js({.numWorkers = 4});
    std::atomic<int> counter{0};

    js.schedule([&]() { counter.fetch_add(1); }, "inc");
    js.waitForAll();

    REQUIRE(counter == 1);
}

TEST_CASE("JobSystem 1M tasks performance") {
    JobSystem js({.numWorkers = 8});
    std::atomic<size_t> counter{0};
    constexpr size_t N = 1'000'000;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        js.schedule([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
        if ((i & 0xFFF) == 0) std::this_thread::yield(); // let workers consume
    }
    js.waitForAll();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    REQUIRE(counter == N);
    REQUIRE(elapsed.count() < 30'000'000'000); // < 30 seconds (CI runner)
}

TEST_CASE("JobSystem multi-thread safety") {
    JobSystem js({.numWorkers = 8});
    std::atomic<size_t> counter{0};
    constexpr size_t THREADS = 10;
    constexpr size_t OPS = 100'000;

    std::vector<std::thread> threads;
    for (size_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&]() {
            for (size_t i = 0; i < OPS; ++i) {
                js.schedule([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
                if ((i & 0x3FF) == 0) std::this_thread::yield();
            }
        });
    }
    for (auto& t : threads) t.join();
    js.waitForAll();

    REQUIRE(counter == THREADS * OPS);
}

TEST_CASE("JobSystem DAG order") {
    JobSystem js;
    std::vector<int> order;
    std::mutex orderMutex;

    auto A = js.createTask([&]() {
        std::lock_guard<std::mutex> g(orderMutex);
        order.push_back(1);
    }, "A");
    auto B = js.createTask([&]() {
        std::lock_guard<std::mutex> g(orderMutex);
        order.push_back(2);
    }, "B");
    auto C = js.createTask([&]() {
        std::lock_guard<std::mutex> g(orderMutex);
        order.push_back(3);
    }, "C");

    js.addDependency(A, B);
    js.addDependency(B, C);
    js.submit(A);   // only root needs explicit submit
    js.waitForAll();

    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
    REQUIRE(order[2] == 3);
}

TEST_CASE("JobSystem parallelFor") {
    JobSystem js({.numWorkers = 4});
    std::vector<int> data(1000, 0);

    js.parallelFor(data.size(), [&](size_t i) {
        data[i] = static_cast<int>(i * 2);
    });

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == static_cast<int>(i * 2));
    }
}

TEST_CASE("JobSystem no deadlock on submit wait cycles") {
    JobSystem js({.numWorkers = 4});
    for (int i = 0; i < 100; ++i) {
        std::atomic<int> counter{0};
        js.schedule([&]() { counter++; });
        js.waitForAll();
        REQUIRE(counter == 1);
    }
}

TEST_CASE("JobSystem TaskGraph submit") {
    JobSystem js({.numWorkers = 4});
    std::atomic<int> value{0};

    TaskGraph graph;
    auto a = graph.createTask([&]() { value.fetch_add(1); }, "A");
    auto b = graph.createTask([&]() { value.fetch_add(10); }, "B");
    graph.addDependency(a, b);
    graph.submit(&js);
    js.waitForAll();

    REQUIRE(value == 11);
}
