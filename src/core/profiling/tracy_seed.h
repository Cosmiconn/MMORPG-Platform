#pragma once

// Minimal Tracy wrapper - no-op if TRACY_ENABLE not defined
#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>
    #define SEED_ZONE(name) ZoneScopedN(name)
    #define SEED_ZONE_COLOR(name, color) ZoneScopedNC(name, color)
    #define SEED_MESSAGE(msg) TracyMessage(msg, strlen(msg))
    #define SEED_PLOT(name, value) TracyPlot(name, value)
#else
    #define SEED_ZONE(name) ((void)0)
    #define SEED_ZONE_COLOR(name, color) ((void)0)
    #define SEED_MESSAGE(msg) ((void)0)
    #define SEED_PLOT(name, value) ((void)0)
#endif
