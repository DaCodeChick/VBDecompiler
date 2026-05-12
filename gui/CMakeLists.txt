cmake_minimum_required(VERSION 3.16)
project(VBDecompilerGUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# UI files directory
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
)

set(HEADERS
    include/MainWindow.h
)

set(UI_FILES
    ui/MainWindow.ui
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bindings/include
)

# Create executable
add_executable(vbdecompiler-gui
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# Link Qt libraries
target_link_libraries(vbdecompiler-gui
    Qt6::Core
    Qt6::Widgets
)

# Link VBDecompiler core library
target_link_libraries(vbdecompiler-gui
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/zig-out/lib/libvbdecomp.so
    sqlite3
    pthread
    dl
    m
)

# Set output directory
set_target_properties(vbdecompiler-gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS vbdecompiler-gui
    RUNTIME DESTINATION bin
)
