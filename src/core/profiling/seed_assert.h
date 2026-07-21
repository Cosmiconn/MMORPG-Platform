#pragma once

#include "core/profiling/crash_handler.h"
#include <cstdio>
#include <cstdlib>

namespace seed {
    inline void seed_assert_impl(bool cond, const char* msg, const char* file, int line) {
#ifdef NDEBUG
        (void)cond; (void)msg; (void)file; (void)line;
#else
        if (!cond) {
            CrashHandler::triggerAssert("ASSERTION FAILED", msg, file, line);
        }
#endif
    }
}

#define SEED_ASSERT(cond, msg) \
    ::seed::seed_assert_impl((cond), (msg), __FILE__, __LINE__)
