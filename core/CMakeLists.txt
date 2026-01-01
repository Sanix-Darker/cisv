cmake_minimum_required(VERSION 3.14)

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
