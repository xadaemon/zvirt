# C port of zvirt. See ../MIGRATION.md.
#
# Configure through a preset, which picks clang and sets every option explicitly:
#
#     cmake --preset debug      # or: tsan, release
#     cmake --build --preset debug
#     ctest --preset debug
#
# To run a subset of tests, run a test executable directly with name filters, from the zvirt/
# root, since tests open fixtures such as test_bins/bzImage by relative path:
#
#     build/debug/zv_test_utils_tests "mmap"

cmake_minimum_required(VERSION 3.25)
project(zvirt LANGUAGES C)

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "zvirt is built with clang; configure with a preset from CMakePresets.json")
endif()

# ---------------------------------------------------------------------------------------------------
# Options. The presets set all of them, so nothing here depends on the build type.
# ---------------------------------------------------------------------------------------------------

set(ZV_SANITIZE "none" CACHE STRING "Sanitizers to build with: none, address (ASan+UBSan), thread")
set_property(CACHE ZV_SANITIZE PROPERTY STRINGS none address thread)
option(ZV_WERROR "Treat warnings as errors" OFF)
option(ZV_PROFILE "Keep frame pointers and unwind tables for perf profiling" OFF)

# ---------------------------------------------------------------------------------------------------
# Language and warnings
# ---------------------------------------------------------------------------------------------------

# Strict ISO C17. Without CMAKE_C_EXTENSIONS OFF, CMake would pass -std=gnu17.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict mode hides Linux and POSIX APIs (pipe2, eventfd, pthread_kill, ...); this brings them back.
add_compile_definitions(_GNU_SOURCE)

add_compile_options(-Wall -Wextra -Wpedantic)
if(ZV_WERROR)
    add_compile_options(-Werror)
endif()

if(ZV_PROFILE)
    add_compile_options(-fno-omit-frame-pointer -fasynchronous-unwind-tables -g)
endif()

# ---------------------------------------------------------------------------------------------------
# Sanitizers. The flags go on both the compile and the link line.
# ---------------------------------------------------------------------------------------------------

if(ZV_SANITIZE STREQUAL "address")
    # -fno-sanitize-recover makes UBSan stop the program instead of printing and carrying on,
    # so undefined behaviour fails the test.
    set(ZV_SANITIZER_FLAGS
        -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
elseif(ZV_SANITIZE STREQUAL "thread")
    set(ZV_SANITIZER_FLAGS -fsanitize=thread)
elseif(ZV_SANITIZE STREQUAL "none")
    set(ZV_SANITIZER_FLAGS "")
else()
    message(FATAL_ERROR "ZV_SANITIZE must be none, address or thread (got '${ZV_SANITIZE}')")
endif()

add_compile_options(${ZV_SANITIZER_FLAGS})
add_link_options(${ZV_SANITIZER_FLAGS})

find_package(Threads REQUIRED)

# Every include is written relative to csrc/, e.g. #include "utils/log.h".
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------------------------------
# Libraries. One per Zig module in build.zig.
# ---------------------------------------------------------------------------------------------------

add_library(zv_utils STATIC
    utils/epoll.c
    utils/eventfd.c
    utils/idalloc.c
    utils/log.c
    utils/mac.c
    utils/tap.c
)
target_link_libraries(zv_utils PUBLIC Threads::Threads)

add_library(zv_test_utils STATIC
    test_utils/mmap.c
    test_utils/test_utils.c
)
target_link_libraries(zv_test_utils PUBLIC zv_utils Threads::Threads)

# ---------------------------------------------------------------------------------------------------
# Tests. One executable per Zig test step in build.zig.
# ---------------------------------------------------------------------------------------------------

enable_testing()

# Tests open fixtures (test_bins/, initrd images) relative to the zvirt/ root, like the Zig tests.
set(ZV_TEST_WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/..)

add_library(zv_test_runner STATIC
    test_runner.c
)
target_link_libraries(zv_test_runner PUBLIC zv_utils)

add_executable(zv_utils_tests
    utils/tests_main.c
    utils/epoll_test.c
    utils/eventfd_test.c
    utils/idalloc_test.c
    utils/mac_test.c
    utils/tap_test.c
)
target_link_libraries(zv_utils_tests PRIVATE zv_utils zv_test_runner)
add_test(NAME utils COMMAND zv_utils_tests WORKING_DIRECTORY ${ZV_TEST_WORKING_DIRECTORY})

add_executable(zv_test_utils_tests
    test_utils/tests_main.c
    test_utils/mmap_test.c
    test_utils/test_utils_test.c
)
target_link_libraries(zv_test_utils_tests PRIVATE zv_test_utils zv_test_runner)
add_test(NAME test_utils COMMAND zv_test_utils_tests WORKING_DIRECTORY ${ZV_TEST_WORKING_DIRECTORY})
