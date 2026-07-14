find_package(doctest CONFIG REQUIRED)

add_executable(seed_tests
    unit/main.cpp
    unit/memory/test_block_allocator.cpp
    unit/memory/test_pool_allocator.cpp
    unit/memory/test_arena_allocator.cpp
    unit/memory/test_stack_allocator.cpp
    unit/memory/test_memory_tracker.cpp
    unit/memory/test_integration.cpp
    # ECS tests
    unit/ecs/test_ecs_basics.cpp
    unit/ecs/test_ecs_performance.cpp
    # Diagnostics tests
    unit/diagnostics/test_diagnostics.cpp
)

target_link_libraries(seed_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_tests)
endif()

include(doctest)
doctest_discover_tests(seed_tests)

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
add_executable(seed_bench_block
    benchmarks/bench_block_allocator.cpp
)
target_link_libraries(seed_bench_block PRIVATE seed_core)
seed_apply_warnings(seed_bench_block)

add_executable(seed_bench_pool
    benchmarks/bench_pool_allocator.cpp
)
target_link_libraries(seed_bench_pool PRIVATE seed_core)
seed_apply_warnings(seed_bench_pool)

add_executable(seed_bench_arena
    benchmarks/bench_arena_allocator.cpp
)
target_link_libraries(seed_bench_arena PRIVATE seed_core)
seed_apply_warnings(seed_bench_arena)

add_executable(seed_bench_integration
    benchmarks/bench_integration.cpp
)
target_link_libraries(seed_bench_integration PRIVATE seed_core)
seed_apply_warnings(seed_bench_integration)

# ECS benchmark
add_executable(seed_bench_ecs
    benchmarks/bench_ecs.cpp
)
target_link_libraries(seed_bench_ecs PRIVATE seed_core)
seed_apply_warnings(seed_bench_ecs)
