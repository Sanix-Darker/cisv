cmake_minimum_required(VERSION 3.15...3.27)
project(cisv_python LANGUAGES C CXX)

# C++ standard for nanobind
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find Python first to determine if we're in pip install mode
find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

# Check if we're building as a standalone package (pip install) or as part of cisv
if(SKBUILD)
    # Building via scikit-build-core (pip install)
    message(STATUS "Building via scikit-build-core")

    # Use FetchContent to get nanobind
    include(FetchContent)
    FetchContent_Declare(
        nanobind
        GIT_REPOSITORY https://github.com/wjakob/nanobind.git
        GIT_TAG v2.2.0
    )
    FetchContent_MakeAvailable(nanobind)

    # Build the core library from source
    set(CISV_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../core")

    # Add core library sources directly
    add_library(cisv_core STATIC
        ${CISV_CORE_DIR}/src/parser.c
    )
    target_include_directories(cisv_core PUBLIC
        ${CISV_CORE_DIR}/include
    )
    target_compile_options(cisv_core PRIVATE
        -O3 -ffast-math -funroll-loops
    )

    # Enable SIMD if available
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        target_compile_options(cisv_core PRIVATE -mavx2)
    endif()

    check_c_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
        target_compile_options(cisv_core PRIVATE -march=native -mtune=native)
    endif()

    # Link pthread for parallel parsing
    find_package(Threads REQUIRED)
    target_link_libraries(cisv_core PRIVATE Threads::Threads)

else()
    # Building as part of the main cisv project
    message(STATUS "Building as part of cisv project")

    # Try to find nanobind via find_package first
    find_package(nanobind CONFIG QUIET)
    if(NOT nanobind_FOUND)
        # Fall back to FetchContent
        include(FetchContent)
        FetchContent_Declare(
            nanobind
            GIT_REPOSITORY https://github.com/wjakob/nanobind.git
            GIT_TAG v2.2.0
        )
        FetchContent_MakeAvailable(nanobind)
    endif()

    # Use the existing cisv target from the parent project
    if(NOT TARGET cisv)
        message(FATAL_ERROR "cisv target not found. Build from the project root.")
    endif()

    # Create alias for consistency
    add_library(cisv_core ALIAS cisv)
endif()

# Create the Python extension module
nanobind_add_module(_core
    STABLE_ABI
    NB_STATIC
    src/cisv_nanobind.cpp
)

# Link against the core library
if(SKBUILD)
    target_link_libraries(_core PRIVATE cisv_core)
    target_include_directories(_core PRIVATE
        ${CISV_CORE_DIR}/include
    )
else()
    target_link_libraries(_core PRIVATE cisv)
    target_include_directories(_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
    )
endif()

# Set output directory for the module
if(SKBUILD)
    # For scikit-build-core: install to the cisv package directory
    install(TARGETS _core LIBRARY DESTINATION cisv)
else()
    # For development: output to the cisv package directory
    set_target_properties(_core PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cisv"
    )
endif()

# Compiler optimizations
target_compile_options(_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3>
)
