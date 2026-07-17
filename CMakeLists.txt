cmake_minimum_required(VERSION 3.25)
project(TheSeed VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compiler_warnings)
include(sanitizers)

# =============================================================================
# Dependencies (vcpkg)
# =============================================================================
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# Tracy profiling is OFF by default (see P0-M1 BuildConfig / P0-M2 risk table:
# Tracy instrumentation ~halves allocation throughput and belongs in dedicated
# profiling builds). Enable with: -DSEED_ENABLE_TRACY=ON
#
# The Tracy CMake package (vcpkg port "tracy" and upstream) is named "Tracy"
# and exports the target Tracy::TracyClient.
option(SEED_ENABLE_TRACY "Enable Tracy profiling instrumentation" OFF)
if(SEED_ENABLE_TRACY)
  find_package(Tracy CONFIG)
  if(Tracy_FOUND)
    message(STATUS "Tracy found – profiling instrumentation enabled")
  else()
    message(WARNING "SEED_ENABLE_TRACY=ON but Tracy package not found – continuing without Tracy")
  endif()
endif()
find_package(doctest CONFIG REQUIRED)

# =============================================================================
# Core Library
# =============================================================================
add_library(seed_core STATIC
    src/core/memory/block_allocator.cpp
    src/core/memory/arena_allocator.cpp
    src/core/memory/stack_allocator.cpp
    src/core/memory/memory_tracker.cpp
    src/core/memory/memory_system.cpp
    # ECS (Month 3)
    src/core/ecs/world.cpp
    src/core/ecs/archetype.cpp
    src/core/ecs/type_registry.cpp
    src/core/ecs/archetype_manager.cpp
    # Diagnostics (P0-M3 extension)
    src/core/diagnostics/event_timeline.cpp
    src/core/diagnostics/health_score.cpp
    src/core/diagnostics/ecs_validator.cpp
    src/core/diagnostics/memory_validator.cpp
    src/core/diagnostics/snapshot_dump.cpp
    src/core/diagnostics/diagnostics_manager.cpp
    # Job-System (Month 4)
    src/core/jobs/work_stealing_queue.cpp
    src/core/jobs/task.cpp
    src/core/jobs/task_graph.cpp
    src/core/jobs/job_system.cpp
)

target_include_directories(seed_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(seed_core PUBLIC
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
)

if(SEED_ENABLE_TRACY AND Tracy_FOUND)
  target_link_libraries(seed_core PUBLIC Tracy::TracyClient)
  target_compile_definitions(seed_core PUBLIC TRACY_ENABLE)
endif()

seed_apply_warnings(seed_core)

# =============================================================================
# Executables
# =============================================================================
add_executable(seed_smoke src/main.cpp)
target_link_libraries(seed_smoke PRIVATE seed_core)
seed_apply_warnings(seed_smoke)

# =============================================================================
# Tests
# =============================================================================
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS seed_smoke seed_core
  EXPORT TheSeedTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
