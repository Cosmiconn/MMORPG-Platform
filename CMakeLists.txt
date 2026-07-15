cmake_minimum_required(VERSION 3.25)
project(TheSeed VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Build Info – injected into every translation unit
# ---------------------------------------------------------------------------
# Git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SEED_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT SEED_GIT_COMMIT)
    set(SEED_GIT_COMMIT "unknown")
endif()

# Git branch
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SEED_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT SEED_GIT_BRANCH)
    set(SEED_GIT_BRANCH "unknown")
endif()

# CI detection
if(DEFINED ENV{CI})
    set(SEED_CI_BUILD "1")
    set(SEED_CI_PLATFORM "$ENV{RUNNER_OS}")
else()
    set(SEED_CI_BUILD "0")
    set(SEED_CI_PLATFORM "local")
endif()

# Compiler info
set(SEED_COMPILER_INFO "${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")
set(SEED_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

message(STATUS "TheSeed Build Info:")
message(STATUS "  Git: ${SEED_GIT_COMMIT} @ ${SEED_GIT_BRANCH}")
message(STATUS "  Compiler: ${SEED_COMPILER_INFO}")
message(STATUS "  Platform: ${SEED_PLATFORM}")
message(STATUS "  CI: ${SEED_CI_BUILD} (${SEED_CI_PLATFORM})")

# ---------------------------------------------------------------------------
# vcpkg integration
# ---------------------------------------------------------------------------
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(doctest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Optional: Tracy
find_package(Tracy CONFIG QUIET)
if(Tracy_FOUND)
    set(SEED_TRACY_ENABLED 1)
    message(STATUS "Tracy profiling enabled")
else()
    set(SEED_TRACY_ENABLED 0)
    message(STATUS "Tracy not found, profiling disabled")
endif()

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(seed_core STATIC
    src/core/memory/allocator.h
    src/core/memory/block_allocator.cpp
    src/core/memory/block_allocator.h
    src/core/memory/memory_system.cpp
    src/core/memory/memory_system.h
    src/core/memory/memory_tracker.cpp
    src/core/memory/memory_tracker.h
    src/core/memory/arena_allocator.cpp
    src/core/memory/arena_allocator.h
    src/core/memory/pool_allocator.h
    src/core/memory/stack_allocator.cpp
    src/core/memory/stack_allocator.h

    src/core/ecs/entity.h
    src/core/ecs/component_traits.h
    src/core/ecs/component_array.h
    src/core/ecs/archetype.h
    src/core/ecs/archetype.cpp
    src/core/ecs/archetype_manager.h
    src/core/ecs/archetype_manager.cpp
    src/core/ecs/type_registry.h
    src/core/ecs/type_registry.cpp
    src/core/ecs/query.h
    src/core/ecs/system.h
    src/core/ecs/world.h
    src/core/ecs/world.cpp

    src/core/profiling/seed_assert.h
    src/core/profiling/tracy_seed.h

    src/core/diagnostics/diagnostics_config.h
    src/core/diagnostics/health_score.h
    src/core/diagnostics/health_score.cpp
    src/core/diagnostics/event_timeline.h
    src/core/diagnostics/event_timeline.cpp
    src/core/diagnostics/ecs_validator.h
    src/core/diagnostics/ecs_validator.cpp
    src/core/diagnostics/snapshot_dump.h
    src/core/diagnostics/snapshot_dump.cpp
    src/core/diagnostics/diagnostics_manager.h
    src/core/diagnostics/diagnostics_manager.cpp
)

target_include_directories(seed_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(seed_core PUBLIC
    fmt::fmt
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

if(Tracy_FOUND)
    target_link_libraries(seed_core PUBLIC Tracy::TracyClient)
endif()

# ---------------------------------------------------------------------------
# Build-time definitions (injected into ALL targets)
# ---------------------------------------------------------------------------
add_compile_definitions(
    SEED_GIT_COMMIT="${SEED_GIT_COMMIT}"
    SEED_GIT_BRANCH="${SEED_GIT_BRANCH}"
    SEED_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    SEED_COMPILER_INFO="${SEED_COMPILER_INFO}"
    SEED_PLATFORM="${SEED_PLATFORM}"
    SEED_CI_BUILD=${SEED_CI_BUILD}
    SEED_CI_PLATFORM="${SEED_CI_PLATFORM}"
)

# Debug/Release specific
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(seed_core PUBLIC SEED_DEBUG=1)
    target_compile_options(seed_core PUBLIC -O0 -g3)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(seed_core PUBLIC SEED_DEBUG=0)
    target_compile_options(seed_core PUBLIC -O3)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(seed_core PUBLIC SEED_DEBUG=0)
endif()

# Sanitizers for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    target_compile_options(seed_core PUBLIC -fsanitize=address,undefined)
    target_link_options(seed_core PUBLIC -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(seed_core PUBLIC /W4 /WX)
else()
    target_compile_options(seed_core PUBLIC -Wall -Wextra -Wpedantic -Werror)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(seed_app src/main.cpp)
target_link_libraries(seed_app PRIVATE seed_core)
