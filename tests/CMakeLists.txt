find_package(doctest CONFIG REQUIRED)

add_executable(seed_tests
    unit/main.cpp
    # ... weitere Test-Dateien
)

target_link_libraries(seed_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_tests)

include(doctest)
doctest_discover_tests(seed_tests)
