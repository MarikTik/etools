# tests/CMakeLists.txt

include(GoogleTest)

# Use GLOB_RECURSE to find all C++ source files in the current directory and its subdirectories.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Define a list of test files that should be compiled with assertions disabled.
set(NDEBUG_TEST_SOURCES
    memory/test_envelope.cpp
)

foreach(test_src ${TEST_SOURCES})
    # Extract the base name of the test file (e.g., "test_envelope")
    get_filename_component(test_name ${test_src} NAME_WE)

    # Get the path of the source file relative to the project root.
    file(RELATIVE_PATH test_src_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${test_src}")

    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE etools GTest::gtest_main)
  
    # Check if the current test file's relative path is in the list of NDEBUG sources.
    list(FIND NDEBUG_TEST_SOURCES ${test_src_rel} index)
    if(index GREATER_EQUAL 0)
        message(STATUS "Compiling test '${test_name}' with assertions disabled (NDEBUG).")
        target_compile_definitions(${test_name} PRIVATE NDEBUG)
    endif()
    
    gtest_discover_tests(${test_name})
endforeach()
