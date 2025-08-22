# tests/CMakeLists.txt

include(GoogleTest)

# Use GLOB_RECURSE to find all C++ source files in the current directory and its subdirectories.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Define a list of test files that should be compiled with assertions disabled.
set(NDEBUG_TEST_SOURCES
    memory/test_envelope.cpp
    memory/test_envelope_view.cpp
)
 
set(GNU_OPTIONS
    -fconstexpr-ops-limit=2147483647     # 2^28; raise if needed
    -fconstexpr-loop-limit=2147483647   # effectively "no limit" on loop iterations
)

set(CLANG_OPTIONS
    -fconstexpr-steps=2147483647
)

set(MSVC_OPTIONS
    /constexpr:depth2147483647
)
foreach(test_src ${TEST_SOURCES})
    # Extract the base name of the test file (e.g., "test_envelope")
    get_filename_component(test_name ${test_src} NAME_WE)

    # Get the path of the source file relative to the project root.
    file(RELATIVE_PATH test_src_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${test_src}")

    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE etools GTest::gtest_main)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC
        target_compile_options(${test_name} PRIVATE ${GNU_OPTIONS})
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Clang/AppleClang
        target_compile_options(${test_name} PRIVATE ${CLANG_OPTIONS})

    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # MSVC
        target_compile_options(${test_name} PRIVATE ${MSVC_OPTIONS})
    endif()
    
    list(FIND NDEBUG_TEST_SOURCES ${test_src_rel} index)
    if(index GREATER_EQUAL 0)
        message(STATUS "Compiling test '${test_name}' with assertions disabled (NDEBUG).")
        target_compile_definitions(${test_name} PRIVATE NDEBUG)
        # Apply release-level optimizations for NDEBUG tests.
        target_compile_options(${test_name} PRIVATE -O3)
        target_link_options(${test_name} PRIVATE -O3)
    else()
        # Compile all other tests with full debug symbols and no optimizations.
        message(STATUS "Compiling test '${test_name}' with full debug info.")
        target_compile_options(${test_name} PRIVATE -g -O0)
    endif()
    
    gtest_discover_tests(${test_name})
endforeach()