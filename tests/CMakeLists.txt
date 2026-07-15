# Test executable
add_executable(seed_tests
    unit/ecs/test_ecs_basics.cpp
    unit/ecs/test_ecs_performance.cpp
    unit/memory/test_block_allocator.cpp
    unit/diagnostics/test_diagnostics.cpp
    main.cpp
)

target_link_libraries(seed_tests PRIVATE
    seed_core
    doctest::doctest
)

target_compile_definitions(seed_tests PRIVATE
    SEED_DIAGNOSTICS_ENABLED=1
    DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

# Register tests with CTest
include(doctest)
doctest_discover_tests(seed_tests)
