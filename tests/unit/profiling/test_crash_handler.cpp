#include <doctest/doctest.h>
#include "core/profiling/crash_handler.h"

using namespace seed;

TEST_SUITE("test_crash_handler") {

TEST_CASE("CrashHandler_StackTrace_NonEmpty") {
    auto trace = CrashHandler::getStackTrace();
    REQUIRE(!trace.empty());
    REQUIRE(trace.find("Stack trace") != std::string::npos);
}

} // TEST_SUITE
