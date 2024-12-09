cmake_minimum_required(VERSION 3.14)
project(skiplist)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add ThreadSanitizer build type
set(CMAKE_CXX_FLAGS_TSAN
    "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread -g -O2"
    CACHE STRING "Flags used by the C++ compiler for ThreadSanitizer build type."
    FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TSAN
    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=thread"
    CACHE STRING "Flags used by the linker for ThreadSanitizer build type."
    FORCE)

# Add compiler optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")
    # Enable Link Time Optimization
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
endif()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Main libraries
add_library(skiplist INTERFACE)
target_include_directories(skiplist INTERFACE include)

add_library(skiplist_atomic INTERFACE)
target_include_directories(skiplist_atomic INTERFACE include)

# Unit tests
enable_testing()
add_executable(skiplist_test tests/unit_tests/skiplist_test.cpp)
target_link_libraries(skiplist_test PRIVATE skiplist gtest_main gmock)
add_test(NAME skiplist_test COMMAND skiplist_test)

add_executable(concurrent_load_test tests/load_tests/concurrent_load_test.cpp)
target_link_libraries(concurrent_load_test PRIVATE skiplist_atomic)

add_executable(concurrent_correctness_test tests/load_tests/concurrent_correctness_test.cpp)
target_link_libraries(concurrent_correctness_test PRIVATE skiplist_atomic)
