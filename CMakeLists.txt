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
find_package(TracyClient CONFIG)
if(TracyClient_FOUND)
  message(STATUS "Tracy found")
else()
  message(STATUS "Tracy not found")
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

if(TracyClient_FOUND)
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
