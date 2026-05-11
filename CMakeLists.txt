cmake_minimum_required(VERSION 3.20)
project(VBDecompiler VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_GUI "Build Qt GUI frontend" ON)
option(BUILD_CLI "Build CLI tool" ON)
option(BUILD_TESTS "Build tests" ON)

# Find Zig compiler
find_program(ZIG_COMPILER zig REQUIRED)
if(NOT ZIG_COMPILER)
    message(FATAL_ERROR "Zig compiler not found. Please install Zig 0.13.0 or later.")
endif()

message(STATUS "Found Zig compiler: ${ZIG_COMPILER}")

# Detect build type for Zig
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ZIG_OPTIMIZE "Debug")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(ZIG_OPTIMIZE "ReleaseSafe")
else()
    set(ZIG_OPTIMIZE "ReleaseFast")
endif()

# Build Zig core library
set(VBDECOMP_LIB_DIR ${CMAKE_SOURCE_DIR}/core/zig-out/lib)
if(WIN32)
    set(VBDECOMP_SHARED_LIB ${VBDECOMP_LIB_DIR}/vbdecomp.dll)
    set(VBDECOMP_STATIC_LIB ${VBDECOMP_LIB_DIR}/vbdecomp.lib)
elseif(APPLE)
    set(VBDECOMP_SHARED_LIB ${VBDECOMP_LIB_DIR}/libvbdecomp.dylib)
    set(VBDECOMP_STATIC_LIB ${VBDECOMP_LIB_DIR}/libvbdecomp.a)
else()
    set(VBDECOMP_SHARED_LIB ${VBDECOMP_LIB_DIR}/libvbdecomp.so)
    set(VBDECOMP_STATIC_LIB ${VBDECOMP_LIB_DIR}/libvbdecomp.a)
endif()

add_custom_command(
    OUTPUT ${VBDECOMP_SHARED_LIB}
    COMMAND ${ZIG_COMPILER} build -Doptimize=${ZIG_OPTIMIZE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/core
    COMMENT "Building Zig decompiler core library (${ZIG_OPTIMIZE})"
    VERBATIM
)

add_custom_target(vbdecomp_core ALL
    DEPENDS ${VBDECOMP_SHARED_LIB}
)

# Create imported library target
add_library(vbdecomp SHARED IMPORTED GLOBAL)
set_target_properties(vbdecomp PROPERTIES
    IMPORTED_LOCATION ${VBDECOMP_SHARED_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/bindings/include
)
add_dependencies(vbdecomp vbdecomp_core)

# Add GUI subdirectory if enabled
if(BUILD_GUI)
    add_subdirectory(gui)
endif()

# Install
install(FILES ${VBDECOMP_SHARED_LIB}
        DESTINATION lib
        COMPONENT libraries)

install(FILES ${CMAKE_SOURCE_DIR}/bindings/include/vbdecomp.h
        DESTINATION include
        COMPONENT headers)

if(BUILD_CLI)
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/core/zig-out/bin/vbdecomp
            DESTINATION bin
            COMPONENT cli
            OPTIONAL)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "VBDecompiler Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Zig optimize: ${ZIG_OPTIMIZE}")
message(STATUS "  Build GUI: ${BUILD_GUI}")
message(STATUS "  Build CLI: ${BUILD_CLI}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "")
