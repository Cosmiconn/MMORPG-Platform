#pragma once

#include <cstdlib>
#include <fmt/format.h>

// SEED_ASSERT – hard stop in debug, logs in release
#ifdef SEED_DEBUG
    #define SEED_ASSERT(cond, msg) \
        do { \
            if (!(cond)) { \
                fmt::print(stderr, "\n[ASSERTION FAILED] {}:{}\n  Condition: {}\n  Message: {}\n", \
                           __FILE__, __LINE__, #cond, msg); \
                std::abort(); \
            } \
        } while(0)
#else
    #define SEED_ASSERT(cond, msg) ((void)0)
#endif

// SEED_VERIFY – always logs, never aborts (production safety net)
#define SEED_VERIFY(cond, msg) \
    do { \
        if (!(cond)) { \
            fmt::print(stderr, "\n[VERIFY FAILED] {}:{}\n  Condition: {}\n  Message: {}\n", \
                       __FILE__, __LINE__, #cond, msg); \
        } \
    } while(0)
