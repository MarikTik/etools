# Top-level CMakeLists.txt for etools
cmake_minimum_required(VERSION 3.20)
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

# Constant-evaluation budget.
#
# `optimal_mph` builds its lookup table at compile time, and for a large key set
# the FKS backend's construction exceeds every compiler's default budget for
# constant evaluation. The failure is not a diagnostic about the user's code -
# it reads as "must be initialized by a constant expression" pointing into
# fks.hpp - so a consumer has no way to guess the remedy.
#
# Measured: a 260-key table is rejected by Clang 21 at its default 1,048,576
# steps. GCC's default limit is higher and is not reached at that size, but the
# same wall exists further out, so both are raised here.
#
# These are INTERFACE options: the tests' own CMakeLists raised them for the
# test targets only, which left every downstream project to rediscover the
# problem. Anything linking `etools` now inherits the budget it needs.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(etools INTERFACE -fconstexpr-steps=2147483647)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(etools INTERFACE
        -fconstexpr-ops-limit=2147483647
        -fconstexpr-loop-limit=2147483647)
elseif(MSVC)
    target_compile_options(etools INTERFACE /constexpr:depth2147483647)
endif()

# ---------------------------------------------------------------------------------------
# Dependencies (FetchContent): eser
# Keep third-party fetches explicit and minimal.
# ---------------------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    eser
    GIT_REPOSITORY https://github.com/MarikTik/eser.git
    GIT_TAG v1.1.2
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
