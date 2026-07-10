find_package(doctest CONFIG REQUIRED)

add_executable(seed_tests
    unit/main.cpp
    unit/memory/test_block_allocator.cpp
)

target_link_libraries(seed_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_tests)

# Enable sanitizers on tests in debug builds
if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_tests)
endif()

include(doctest)
doctest_discover_tests(seed_tests)

# ---------------------------------------------------------------------------
# Benchmarks (not part of CTest)
# ---------------------------------------------------------------------------
add_executable(seed_bench_block
    benchmarks/bench_block_allocator.cpp
)
target_link_libraries(seed_bench_block PRIVATE seed_core)
seed_apply_warnings(seed_bench_block)
