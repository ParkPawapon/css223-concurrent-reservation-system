cmake_minimum_required(VERSION 3.28)

file(REAL_PATH "${CMAKE_SOURCE_DIR}" CSS223_SOURCE_DIRECTORY)
file(REAL_PATH "${CMAKE_BINARY_DIR}" CSS223_BINARY_DIRECTORY)

if(CSS223_SOURCE_DIRECTORY STREQUAL CSS223_BINARY_DIRECTORY)
    message(FATAL_ERROR
        "In-source builds are not supported. "
        "Use a build directory or run: cmake --preset debug"
    )
endif()

project(
    css223_concurrent_reservation_system
    VERSION 0.1.0
    DESCRIPTION "Concurrent reservation system for the CSS223 Operating Systems course"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(
    CMAKE_EXPORT_COMPILE_COMMANDS ON
    CACHE BOOL "Generate compile_commands.json for supported generators"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_TESTING "Build the CSS223 test suites" OFF)
include(CTest)

include(ProjectOptions)

css223_setup_project_options()

if(BUILD_TESTING)
    include(Testing)
    add_subdirectory(tests)
endif()
