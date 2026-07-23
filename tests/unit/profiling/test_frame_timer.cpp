#include <doctest/doctest.h>
#include "core/profiling/frame_timer.h"
#include <thread>

using namespace seed;

TEST_SUITE("test_frame_timer") {

TEST_CASE("FrameTimer_BudgetAlarm") {
    FrameTimer timer;
    timer.beginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.endFrame();
    REQUIRE(timer.isOverBudget(16.667f) == true);
    REQUIRE(timer.getCurrentStats().deltaTime > 16.667f);
}

TEST_CASE("FrameTimer_AverageFps") {
    FrameTimer timer;
    for (int i = 0; i < 10; ++i) {
        timer.beginFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        timer.endFrame();
    }
    float fps = timer.averageFps();
    REQUIRE(fps > 50.0f);
    REQUIRE(fps < 200.0f);
}

} // TEST_SUITE
