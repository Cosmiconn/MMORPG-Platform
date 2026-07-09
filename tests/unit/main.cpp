#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
TEST_CASE("BuildSystem_Smoke") { REQUIRE(true); }
TEST_CASE("BuildSystem_Cpp20_Concepts") {
  template<typename T> concept Numeric = std::is_arithmetic_v<T>;
  static_assert(Numeric<int>);
  static_assert(Numeric<float>);
  static_assert(!Numeric<void>);
}
