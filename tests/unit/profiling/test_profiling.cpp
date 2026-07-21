#include <doctest/doctest.h>
#include "core/profiling/frame_timer.h"
#include "core/profiling/crash_handler.h"
#include <thread>

using namespace seed;

TEST_CASE("FrameTimer_BasicTiming") {
    FrameTimer timer;
    timer.beginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    timer.endFrame();
    CHECK(timer.getCurrentStats().deltaTime >= 4.0f);
}

TEST_CASE("FrameTimer_BudgetAlarm") {
    FrameTimer timer;
    timer.beginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.endFrame();
    CHECK(timer.isOverBudget(16.67f) == true);
    CHECK(timer.getCurrentStats().deltaTime > 16.67f);
}

TEST_CASE("FrameTimer_AverageFps") {
    FrameTimer timer;
    for (int i = 0; i < 5; ++i) {
        timer.beginFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        timer.endFrame();
    }
    float fps = timer.averageFps();
    CHECK(fps > 0.0f);
    CHECK(fps < 1000.0f);
}

TEST_CASE("FrameTimer_Percentile") {
    FrameTimer timer;
    for (int i = 0; i < 10; ++i) {
        timer.beginFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(i + 1));
        timer.endFrame();
    }
    float p50 = timer.percentileFrameTime(0.5f);
    CHECK(p50 > 0.0f);
}

TEST_CASE("CrashHandler_StackTrace_NonEmpty") {
    auto trace = CrashHandler::getStackTrace();
    CHECK(!trace.empty());
    CHECK(trace.find("Stack trace") != std::string::npos);
}
