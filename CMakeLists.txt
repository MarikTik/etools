# Top-level CMakeLists.txt for etools
cmake_minimum_required(VERSION 3.2)
project(etools LANGUAGES CXX)

# ---------------------------------------------------------------------------------------
# Project policy: out-of-source builds only (prevents accidental clutter)
# ---------------------------------------------------------------------------------------
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed. Use: cmake -S . -B build")
endif()

# ---------------------------------------------------------------------------------------
# C++ standard and basic target: etools is header-only
# ---------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(etools INTERFACE)

# Public include dirs:
# - During build, expose the repo root so consumers can include <etools/...>
# - During install, headers go to include/etools so includes remain stable
target_include_directories(etools INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/etools>
)

# Reasonable warnings for GCC/Clang consumers (propagated to dependents)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(etools INTERFACE -Wall -Wextra -pedantic)
endif()

# ---------------------------------------------------------------------------------------
# Dependencies (FetchContent): eser
# Keep third-party fetches explicit and minimal.
# ---------------------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    eser
    GIT_REPOSITORY https://github.com/MarikTik/eser.git
    GIT_TAG main
)
FetchContent_MakeAvailable(eser)

target_link_libraries(etools INTERFACE eser)

# ---------------------------------------------------------------------------------------
# Testing (gated): default ON only when etools is the top-level project.
# Also respects CTest's BUILD_TESTING.
# ---------------------------------------------------------------------------------------

# CMake 3.21+ provides PROJECT_IS_TOP_LEVEL; add a fallback for older CMake.
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL TRUE)
    else()
        set(PROJECT_IS_TOP_LEVEL FALSE)
    endif()
endif()

# Tests default: ON when building this project directly, OFF when used as a dependency.
set(ETOOLS_BUILD_TESTS_DEFAULT ${PROJECT_IS_TOP_LEVEL})
option(ETOOLS_BUILD_TESTS "Build unit tests for etools" ${ETOOLS_BUILD_TESTS_DEFAULT})

# CTest integration (provides BUILD_TESTING cache var)
include(CTest)

if(ETOOLS_BUILD_TESTS AND BUILD_TESTING)
    enable_testing()

    # Fetch GoogleTest only when tests are enabled.
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)

    # Keep tests out of the default 'all' target to avoid surprising consumers.
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------------------
# Installation: install all public headers under include/etools/
# Consumers will include as: #include <etools/xx/yy.hpp>
# ---------------------------------------------------------------------------------------
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/etools
    FILES_MATCHING
        PATTERN "*.hpp"
)
