#pragma once

#include <cstdlib>
#include <fmt/format.h>

// PUNKT 7: Sanitizer hook integration
#if defined(__clang__) || defined(__GNUC__)
    #define SEED_SANITIZER_HOOK __attribute__((no_sanitize("address,undefined")))
#else
    #define SEED_SANITIZER_HOOK
#endif

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

// SEED_SANITIZER_REPORT – reports sanitizer errors to diagnostics
#define SEED_SANITIZER_REPORT(msg) \
    do { \
        fmt::print(stderr, "\n[SANITIZER] {}:{}\n  {}\n", __FILE__, __LINE__, msg); \
    } while(0)
