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

# Source files (only existing files)
set(CORE_SOURCES
    src/core/pe/PEFile.cpp
    src/core/vb/VBFile.cpp
    src/core/disasm/x86/X86Instruction.cpp
    src/core/disasm/x86/X86Disassembler.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/MainWindow.ui
)

# Main executable
add_executable(vbdecompiler
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)

# Link Qt libraries
target_link_libraries(vbdecompiler PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# Include directories
target_include_directories(vbdecompiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler options
if(MSVC)
    target_compile_options(vbdecompiler PRIVATE /W4)
else()
    target_compile_options(vbdecompiler PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable C++23 features
target_compile_features(vbdecompiler PRIVATE cxx_std_23)

# Test executables
add_executable(test_pe
    src/test_pe.cpp
    src/core/pe/PEFile.cpp
)

target_link_libraries(test_pe PRIVATE
    Qt6::Core
)

target_include_directories(test_pe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(test_vb
    src/test_vb.cpp
    src/core/pe/PEFile.cpp
    src/core/vb/VBFile.cpp
)

target_link_libraries(test_vb PRIVATE
    Qt6::Core
)

target_include_directories(test_vb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(test_x86
    src/test_x86.cpp
    src/core/pe/PEFile.cpp
    src/core/vb/VBFile.cpp
    src/core/disasm/x86/X86Instruction.cpp
    src/core/disasm/x86/X86Disassembler.cpp
)

target_link_libraries(test_x86 PRIVATE
    Qt6::Core
)

target_include_directories(test_x86 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Installation
install(TARGETS vbdecompiler DESTINATION bin)
