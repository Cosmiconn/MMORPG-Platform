cmake_minimum_required(VERSION 3.25)
project(TheSeed VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed.")
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(vcpkg)
include(compiler_warnings)
include(sanitizers)
find_package(doctest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(TracyClient CONFIG)
if(TracyClient_FOUND)
  message(STATUS "Tracy found")
else()
  message(STATUS "Tracy not found")
endif()
add_library(seed_core INTERFACE)
target_include_directories(seed_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
add_executable(seed_smoke src/main.cpp)
target_link_libraries(seed_smoke PRIVATE seed_core)
seed_apply_warnings(seed_smoke)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
install(TARGETS seed_smoke seed_core
  EXPORT TheSeedTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
