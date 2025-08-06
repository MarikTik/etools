# Top-level CMakeLists.txt for etools
cmake_minimum_required(VERSION 3.20)
project(etools LANGUAGES CXX)

# Enforce out-of-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed. Use: cmake -S . -B build")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define etools as a header-only interface library
add_library(etools INTERFACE)
target_include_directories(etools INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Add compiler warnings for consumers
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(etools INTERFACE -Wall -Wextra -pedantic)
endif()

# --- Include eser as a subdirectory ---
include(FetchContent)
FetchContent_Declare(
    eser
    GIT_REPOSITORY https://github.com/MarikTik/eser.git  # <-- update this URL to your actual repo
    GIT_TAG main
)
FetchContent_MakeAvailable(eser)

target_link_libraries(etools INTERFACE eser)

# --- Testing ---
include(CTest)
option(ETOOLS_BUILD_TESTS "Build unit tests for etools" ON)

if(ETOOLS_BUILD_TESTS)
    enable_testing()

    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()

# --- Installation (optional) ---
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")
