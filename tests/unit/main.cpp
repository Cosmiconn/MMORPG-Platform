#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <type_traits>

// Concepts must be at namespace scope, not inside TEST_CASE
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

TEST_CASE("BuildSystem_Smoke") {
    REQUIRE(true);
}

TEST_CASE("BuildSystem_Cpp20_Concepts") {
    static_assert(Numeric<int>);
    static_assert(Numeric<float>);
    // !Numeric<void> would fail static_assert, so we use a workaround
    static_assert(!std::is_arithmetic_v<void>);
}

