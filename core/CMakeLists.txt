cmake_minimum_required(VERSION 3.14)
project(cisv C CXX)

# =============================================================================
# Profile-Guided Optimization (PGO) Support
# Usage:
#   1. cmake -DCISV_PGO_GENERATE=ON .. && make
#   2. Run representative workload
#   3. cmake -DCISV_PGO_USE=ON .. && make
# =============================================================================
option(CISV_PGO_GENERATE "Generate PGO profile data" OFF)
option(CISV_PGO_USE "Use PGO profile data for optimization" OFF)

if(CISV_PGO_GENERATE)
    message(STATUS "PGO: Generating profile data")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-generate")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-generate")
endif()

if(CISV_PGO_USE)
    message(STATUS "PGO: Using profile data for optimization")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-use -fprofile-correction")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-use -fprofile-correction")
endif()

# Core library sources
set(CISV_SOURCES
    src/parser.c
    src/writer.c
    src/transformer.c
)

# Static library
add_library(cisv_static STATIC ${CISV_SOURCES})
target_include_directories(cisv_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(cisv_static PROPERTIES OUTPUT_NAME cisv)

# Shared library
add_library(cisv_shared SHARED ${CISV_SOURCES})
target_include_directories(cisv_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(cisv_shared PROPERTIES OUTPUT_NAME cisv)

# Link pthread for parallel parsing support
find_package(Threads REQUIRED)
target_link_libraries(cisv_static PRIVATE Threads::Threads)
target_link_libraries(cisv_shared PRIVATE Threads::Threads)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_compile_definitions(cisv_static PRIVATE _GNU_SOURCE)
    target_compile_definitions(cisv_shared PRIVATE _GNU_SOURCE)
endif()

if(APPLE)
    target_compile_definitions(cisv_static PRIVATE __APPLE__)
    target_compile_definitions(cisv_shared PRIVATE __APPLE__)
endif()

# Tests
if(CISV_BUILD_TESTS)
    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core PRIVATE cisv_static m pthread)
    target_include_directories(test_core PRIVATE include)

    enable_testing()
    add_test(NAME CoreTests COMMAND test_core)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS cisv_static cisv_shared
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/cisv
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
