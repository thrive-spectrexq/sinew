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

# Internal compiler warning target for repository tests, examples, and benchmarks
add_library(sinew_internal_warnings INTERFACE)
if(MSVC)
    target_compile_options(sinew_internal_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(sinew_internal_warnings INTERFACE -Wall -Wextra -Wpedantic)
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

# Installation & CMake package export
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS sinew
    EXPORT sinewTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT sinewTargets
    FILE sinewTargets.cmake
    NAMESPACE sinew::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sinew
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sinewConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sinewConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sinewConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sinew
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sinewConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sinewConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sinew
)

