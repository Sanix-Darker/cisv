cmake_minimum_required(VERSION 3.14)
project(cisv VERSION 0.0.8 LANGUAGES C CXX)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(CISV_BUILD_CLI "Build CLI tool" ON)
option(CISV_BUILD_TESTS "Build tests" ON)
option(CISV_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(CISV_BUILD_PYTHON "Build Python bindings (nanobind)" OFF)

# Compiler flags - optimized for maximum performance
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -ffast-math -funroll-loops -fomit-frame-pointer")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

# Enable Link-Time Optimization for Release builds
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
if(LTO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    message(STATUS "LTO enabled for Release builds")
else()
    message(STATUS "LTO not supported: ${LTO_ERROR}")
endif()

if(CISV_ENABLE_SIMD)
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2")
    endif()

    check_c_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native")
    endif()
endif()

# Add subdirectories
add_subdirectory(core)

if(CISV_BUILD_CLI)
    add_subdirectory(cli)
endif()

# Python bindings (nanobind)
if(CISV_BUILD_PYTHON)
    add_subdirectory(bindings/python-nanobind)
endif()

# Summary
message(STATUS "")
message(STATUS "CISV Configuration:")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  CLI:         ${CISV_BUILD_CLI}")
message(STATUS "  Tests:       ${CISV_BUILD_TESTS}")
message(STATUS "  SIMD:        ${CISV_ENABLE_SIMD}")
message(STATUS "  Python:      ${CISV_BUILD_PYTHON}")
message(STATUS "")
