#pragma once

// Minimal Tracy wrapper - no-op if TRACY_ENABLE not defined
#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>
    #define SEED_ZONE(name) ZoneScopedN(name)
#else
    #define SEED_ZONE(name) ((void)0)
#endif
