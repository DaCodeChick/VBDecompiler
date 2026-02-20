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

# Source files
set(CORE_PE_SOURCES
    src/core/pe/PEFile.cpp
    src/core/pe/PEHeader.cpp
    src/core/pe/PESection.cpp
    src/core/pe/PEImportTable.cpp
)

set(CORE_VB_SOURCES
    src/core/vb/VBFile.cpp
    src/core/vb/VBHeader.cpp
    src/core/vb/VBProjectInfo.cpp
    src/core/vb/VBObjectTable.cpp
    src/core/vb/VBPublicObjectDescriptor.cpp
)

set(CORE_DISASM_PCODE_SOURCES
    src/core/disasm/pcode/PCodeDisassembler.cpp
    src/core/disasm/pcode/PCodeInstruction.cpp
    src/core/disasm/pcode/PCodeAnalyzer.cpp
)

set(CORE_DISASM_X86_SOURCES
    src/core/disasm/x86/X86Disassembler.cpp
    src/core/disasm/x86/X86Instruction.cpp
)

set(CORE_IR_SOURCES
    src/core/ir/IRFunction.cpp
    src/core/ir/IRBasicBlock.cpp
    src/core/ir/IRStatement.cpp
    src/core/ir/IRExpression.cpp
)

set(CORE_DECOMPILER_SOURCES
    src/core/decompiler/Decompiler.cpp
    src/core/decompiler/VBCodeGenerator.cpp
    src/core/decompiler/TypeRecovery.cpp
    src/core/decompiler/ControlFlowStructurer.cpp
)

set(CORE_SYMBOLS_SOURCES
    src/core/symbols/SymbolTable.cpp
    src/core/symbols/Symbol.cpp
    src/core/symbols/SymbolResolver.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/MainWindow.ui
    src/ui/panels/DisassemblyPanel.cpp
    src/ui/panels/DisassemblyPanel.ui
    src/ui/panels/DecompilerPanel.cpp
    src/ui/panels/DecompilerPanel.ui
    src/ui/panels/SymbolBrowserPanel.cpp
    src/ui/panels/SymbolBrowserPanel.ui
    src/ui/panels/TypeBrowserPanel.cpp
    src/ui/panels/TypeBrowserPanel.ui
    src/ui/panels/CallTreePanel.cpp
    src/ui/panels/CallTreePanel.ui
    src/ui/panels/StringsPanel.cpp
    src/ui/panels/StringsPanel.ui
    src/ui/panels/DataPanel.cpp
    src/ui/panels/DataPanel.ui
    src/ui/panels/FunctionsPanel.cpp
    src/ui/panels/FunctionsPanel.ui
    src/ui/panels/EquatesPanel.cpp
    src/ui/panels/EquatesPanel.ui
    src/ui/widgets/DisassemblyTableWidget.cpp
    src/ui/widgets/VBSyntaxHighlighter.cpp
    src/ui/widgets/AddressLabel.cpp
    src/ui/widgets/HexViewWidget.cpp
)

set(UTILS_SOURCES
    src/utils/Logger.cpp
    src/utils/ByteReader.cpp
    src/utils/UPXDecompressor.cpp
)

# Main executable
add_executable(vbdecompiler
    src/main.cpp
    ${CORE_PE_SOURCES}
    ${CORE_VB_SOURCES}
    ${CORE_DISASM_PCODE_SOURCES}
    ${CORE_DISASM_X86_SOURCES}
    ${CORE_IR_SOURCES}
    ${CORE_DECOMPILER_SOURCES}
    ${CORE_SYMBOLS_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    resources/vbdecompiler.qrc
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler options
if(MSVC)
    target_compile_options(vbdecompiler PRIVATE /W4 /WX)
else()
    target_compile_options(vbdecompiler PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Enable C++23 features
target_compile_features(vbdecompiler PRIVATE cxx_std_23)

# Install targets
install(TARGETS vbdecompiler
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
