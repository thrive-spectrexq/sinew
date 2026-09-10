cmake_minimum_required(VERSION 3.20)
project(sinew VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library target (header-only)
add_library(sinew INTERFACE)
add_library(sinew::sinew ALIAS sinew)

target_include_directories(sinew INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler warnings and options
if(MSVC)
    target_compile_options(sinew INTERFACE /W4 /permissive-)
else()
    target_compile_options(sinew INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Optional components
option(SINEW_BUILD_TESTS "Build Sinew unit test suite" ON)
option(SINEW_BUILD_EXAMPLES "Build Sinew example applications" ON)
option(SINEW_BUILD_BENCHMARKS "Build Sinew micro-benchmarks" ON)

if(SINEW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SINEW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SINEW_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
