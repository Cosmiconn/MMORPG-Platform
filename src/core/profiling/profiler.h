#pragma once

#include "core/profiling/tracy_seed.h"

#ifdef TRACY_ENABLE
    #define SEED_PROFILE_FRAME() FrameMark
    #define SEED_PROFILE_ZONE(name) ZoneScopedN(name)
    #define SEED_PROFILE_ZONE_COLOR(name, color) ZoneScopedNC(name, color)
    #define SEED_PROFILE_TAG(text) TracyMessageL(text)
#else
    #define SEED_PROFILE_FRAME() ((void)0)
    #define SEED_PROFILE_ZONE(name) ((void)0)
    #define SEED_PROFILE_ZONE_COLOR(name, color) ((void)0)
    #define SEED_PROFILE_TAG(text) ((void)0)
#endif
