cmake_minimum_required(VERSION 3.20)
project(VBDecompiler VERSION 1.0.0 LANGUAGES CXX)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Build Rust library first
set(RUST_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/target)
set(RUST_LIB_DIR ${RUST_BUILD_DIR}/$<IF:$<CONFIG:Debug>,debug,release>)

# Determine Rust library name based on platform
if(WIN32)
    set(RUST_FFI_LIB ${RUST_LIB_DIR}/vbdecompiler_ffi.lib)
else()
    set(RUST_FFI_LIB ${RUST_LIB_DIR}/libvbdecompiler_ffi.a)
endif()

# Custom target to build Rust library
add_custom_target(rust_build ALL
    COMMAND cargo build $<IF:$<CONFIG:Debug>,,--release> --package vbdecompiler-ffi
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building Rust FFI library..."
    BYPRODUCTS ${RUST_FFI_LIB}
)

# C++ source files (GUI only - decompilation and x86 disassembly in Rust)
set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/MainWindow.ui
)

# Main executable
add_executable(vbdecompiler
    src/main.cpp
    ${UI_SOURCES}
)

# Make sure Rust is built before linking
add_dependencies(vbdecompiler rust_build)

# Link Qt libraries
target_link_libraries(vbdecompiler PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# Link Rust FFI library
target_link_libraries(vbdecompiler PRIVATE
    ${RUST_FFI_LIB}
)

# On Unix systems, also need to link pthread and dl
if(UNIX AND NOT APPLE)
    target_link_libraries(vbdecompiler PRIVATE
        pthread
        dl
        m
    )
elseif(APPLE)
    target_link_libraries(vbdecompiler PRIVATE
        "-framework Security"
        "-framework CoreFoundation"
    )
elseif(WIN32)
    target_link_libraries(vbdecompiler PRIVATE
        ws2_32
        userenv
        bcrypt
        ntdll
    )
endif()

# Include directories
target_include_directories(vbdecompiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler options
if(MSVC)
    target_compile_options(vbdecompiler PRIVATE /W4)
else()
    target_compile_options(vbdecompiler PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable C++23 features
target_compile_features(vbdecompiler PRIVATE cxx_std_23)

# X86 disassembler test (uses Rust iced-x86 via FFI)
add_executable(test_x86
    tests/test_x86_rust.cpp
)

add_dependencies(test_x86 rust_build)

target_link_libraries(test_x86 PRIVATE
    ${RUST_FFI_LIB}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_x86 PRIVATE pthread dl m)
elseif(APPLE)
    target_link_libraries(test_x86 PRIVATE "-framework Security" "-framework CoreFoundation")
elseif(WIN32)
    target_link_libraries(test_x86 PRIVATE ws2_32 userenv bcrypt ntdll)
endif()

target_include_directories(test_x86 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(test_x86 PRIVATE cxx_std_23)

# Installation
install(TARGETS vbdecompiler DESTINATION bin)
