cmake_minimum_required(VERSION 3.14)

# CLI executable
add_executable(cisv src/main.c)
target_link_libraries(cisv PRIVATE cisv_static m pthread)
target_include_directories(cisv PRIVATE ${CMAKE_SOURCE_DIR}/core/include)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_compile_definitions(cisv PRIVATE _GNU_SOURCE)
    target_link_libraries(cisv PRIVATE rt)
endif()

if(APPLE)
    target_compile_definitions(cisv PRIVATE __APPLE__)
endif()

# Installation
install(TARGETS cisv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
