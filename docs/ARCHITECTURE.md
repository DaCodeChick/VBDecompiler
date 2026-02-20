# VBDecompiler - Architecture

## Overview

VBDecompiler is a Ghidra-inspired decompiler for Visual Basic 5/6 executables. It parses PE files, identifies VB-specific structures, disassembles both P-Code and native x86 code, and reconstructs VB6 source code.

## Technology Stack

### Core: C++23

**Why C++23:**
- Native performance for binary parsing
- Modern C++ features (modules, concepts, ranges, std::expected)
- Direct memory manipulation for PE/binary parsing
- Excellent Qt integration
- Cross-platform compilation

### UI: Qt 6 Widgets

**Why Qt 6:**
- Cross-platform GUI (Windows, Linux, macOS)
- Rich widget library for complex UIs
- `.ui` files for declarative UI design
- Model/View architecture for data tables
- Excellent documentation and tooling

### Build: CMake

**Why CMake:**
- Industry-standard C++ build system
- Excellent Qt 6 integration
- Cross-platform (Windows/Linux/macOS)
- Modern target-based configuration
- IDE integration (VSCode, CLion, Visual Studio)

## Module Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         UI Layer (Qt 6)                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ MainWindow   │  │   Panels     │  │   Widgets    │      │
│  │  (.ui file)  │  │  (.ui files) │  │  (Custom)    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      Core Engine (C++23)                     │
│                                                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │  PE Parser  │  │  VB Parser  │  │   Symbols   │         │
│  └─────────────┘  └─────────────┘  └─────────────┘         │
│                                                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │ P-Code      │  │ x86 Disasm  │  │     IR      │         │
│  │ Disassembler│  │             │  │             │         │
│  └─────────────┘  └─────────────┘  └─────────────┘         │
│                                                               │
│  ┌───────────────────────────────────────────────┐          │
│  │          Decompiler Engine                    │          │
│  │  (IR → VB6 Code Generation)                   │          │
│  └───────────────────────────────────────────────┘          │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Binary File (VB .exe/.dll/.ocx)          │
└─────────────────────────────────────────────────────────────┘
```

## Core Modules

### 1. PE Parser (`src/core/pe/`)

Parses PE (Portable Executable) file format.

**Classes:**
- `PEFile` - Main PE file wrapper
- `PEHeader` - DOS/NT header parsing
- `PESection` - Section handling (.text, .data, .rsrc)
- `PEImportTable` - Import Address Table (IAT) parsing

**Responsibilities:**
- Read DOS header (MZ signature)
- Parse PE header (NT signature, COFF header, optional header)
- Extract sections and section data
- Parse import/export tables
- Map Relative Virtual Addresses (RVAs) to file offsets

**Key Methods:**
```cpp
class PEFile {
public:
    explicit PEFile(const std::filesystem::path& path);
    bool parse();
    
    std::span<const std::byte> getSectionData(std::string_view name) const;
    std::optional<uint32_t> rvaToFileOffset(uint32_t rva) const;
    std::vector<std::string> getImports() const;
};
```

### 2. VB Parser (`src/core/vb/`)

Parses VB5/6 specific binary structures.

**Classes:**
- `VBFile` - Main VB file wrapper
- `VBHeader` - VB5/6 header (VB5! signature)
- `VBProjectInfo` - Project information structure
- `VBObjectTable` - Object table parser
- `VBPublicObjectDescriptor` - Object descriptors

**Responsibilities:**
- Detect VB5/6 signature in binary
- Parse VB header structure
- Extract project information (project name, version, etc.)
- Parse object table (forms, classes, modules)
- Determine P-Code vs native compilation
- Extract method tables and code locations

**Key Methods:**
```cpp
class VBFile {
public:
    explicit VBFile(std::unique_ptr<PEFile> peFile);
    
    bool isVBFile() const;
    bool isPCode() const;
    bool isNativeCode() const;
    
    const VBHeader& getHeader() const;
    const VBProjectInfo& getProjectInfo() const;
    std::vector<VBObject> getObjects() const;
};
```

**Detection Algorithm:**
1. Search for "VB5!" signature in .data or .rdata sections
2. Validate VBHeader structure at signature location
3. Follow `aProjectInfo` pointer to ProjectInfo structure
4. Follow `aObjectTable` pointer to ObjectTable
5. Parse object descriptors and method tables

### 3. P-Code Disassembler (`src/core/disasm/pcode/`)

Disassembles VB P-Code (bytecode).

**Classes:**
- `PCodeDisassembler` - Main disassembler
- `PCodeInstruction` - P-Code instruction representation
- `PCodeAnalyzer` - Control flow analysis

**Responsibilities:**
- Decode P-Code opcodes (256+ opcodes)
- Parse operands (immediate values, offsets, etc.)
- Track stack depth changes
- Identify basic blocks and control flow
- Resolve method calls and jumps

**P-Code Format:**
```
[Opcode: 1 byte] [Operand: 0-4 bytes]
```

**Key Methods:**
```cpp
class PCodeDisassembler {
public:
    std::vector<PCodeInstruction> disassemble(
        std::span<const std::byte> code,
        uint32_t baseAddress
    );
    
private:
    PCodeInstruction decodeInstruction(
        std::span<const std::byte>& bytes,
        size_t& offset
    );
};

struct PCodeInstruction {
    uint32_t address;
    uint8_t opcode;
    std::vector<uint32_t> operands;
    std::string mnemonic;
    int stackDelta;  // Stack depth change (+1, -1, etc.)
};
```

### 4. x86 Disassembler (`src/core/disasm/x86/`)

Disassembles native x86 machine code.

**Classes:**
- `X86Disassembler` - x86 disassembler
- `X86Instruction` - x86 instruction representation

**Responsibilities:**
- Decode x86 instructions (32-bit focus)
- Format Intel syntax
- Resolve jump/call targets
- Track data references

**Note:** Can use external library (Zydis, Capstone) or custom implementation.

### 5. Symbol Manager (`src/core/symbols/`)

Manages symbols (functions, data, strings) with Ghidra-style naming.

**Classes:**
- `SymbolTable` - Main symbol database
- `Symbol` - Symbol representation
- `SymbolResolver` - Symbol resolution and naming

**Responsibilities:**
- Generate default names (`FUN_00401000`, `DAT_00403000`)
- Store user-defined symbol names
- Track symbol types (function, data, string, equate)
- Resolve cross-references (xrefs)

**Naming Convention:**
- Functions: `FUN_<address>` (e.g., `FUN_00401000`)
- Data: `DAT_<address>` (e.g., `DAT_00403000`)
- Strings: `STR_<address>` (e.g., `STR_00404000`)
- User can rename symbols

### 6. Intermediate Representation (`src/core/ir/`)

High-level IR for decompilation.

**Classes:**
- `IRFunction` - Function representation
- `IRBasicBlock` - Basic block (sequential instructions)
- `IRStatement` - Statement (assignment, call, return, etc.)
- `IRExpression` - Expression (binary op, variable, constant, etc.)

**Responsibilities:**
- Lift P-Code/x86 to platform-independent IR
- SSA (Static Single Assignment) form
- Expression trees
- Control flow graph (CFG) construction

**IR Example:**
```
P-Code:      ldloc 0
             ldloc 1
             add
             stloc 2

IR:          v2 = v0 + v1
```

### 7. Decompiler Engine (`src/core/decompiler/`)

Converts IR to VB6 source code.

**Classes:**
- `Decompiler` - Main decompilation orchestrator
- `VBCodeGenerator` - VB6 code generation
- `TypeRecovery` - Type inference engine
- `ControlFlowStructurer` - Reconstruct If/Loop/Select structures

**Responsibilities:**
- Analyze IR control flow
- Infer VB variable types (Integer, Long, String, Variant, etc.)
- Reconstruct high-level control structures
- Generate VB6 syntax

**Decompilation Pipeline:**
1. **IR Analysis** - Analyze data flow and control flow
2. **Type Inference** - Infer types from operations and MSVBVM60 calls
3. **Variable Recovery** - Identify local variables and parameters
4. **Control Flow Structuring** - Convert CFG to If/While/For/Select
5. **Code Generation** - Output VB6 syntax

**Output Example:**
```vb
Function FUN_00401000(arg1 As Integer, arg2 As String) As Long
    Dim local1 As Integer
    Dim local2 As String
    
    local1 = arg1 + 10
    If local1 > 100 Then
        local2 = "Large"
    Else
        local2 = "Small"
    End If
    
    FUN_00401000 = local1
End Function
```

## UI Architecture (Qt 6)

### Main Window (`src/ui/MainWindow.ui`)

Ghidra-style workspace with dockable panels.

**Layout:**
```
┌─────────────────────────────────────────────────────────────┐
│ Menu Bar: File | Edit | View | Tools | Window | Help        │
├─────────────────────────────────────────────────────────────┤
│ Toolbar: [Open] [Save] [Toggle Mode] [Search] ...          │
├───────────┬─────────────────────────────┬──────────────────┤
│           │                             │                  │
│  Symbol   │   Disassembly Listing       │   Call Tree      │
│  Browser  │   (Address | Bytes | Stack  │   ├─ Callers    │
│           │    | Mnemonic | IL)          │   └─ Callees    │
│  ├─ FUN_  │   ─────────────────────────  │                  │
│  ├─ DAT_  │   Decompiler View           │   References     │
│  └─ STR_  │   (VB6 Source Code)         │   (Xrefs)        │
│           │                             │                  │
│  Type     │                             │                  │
│  Browser  │                             │                  │
│           │                             │                  │
├───────────┴─────────────────────────────┴──────────────────┤
│ Bottom Tabs: [Strings] [Data] [Functions] [Equates]        │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│ Status Bar: Ready | PE: OK | VB: Detected | Objects: 15    │
└─────────────────────────────────────────────────────────────┘
```

### UI Panels (`.ui` files)

All panels designed with Qt Designer (.ui files):

1. **DisassemblyPanel.ui** - Disassembly listing table
2. **DecompilerPanel.ui** - Decompiler text view
3. **SymbolBrowserPanel.ui** - Symbol tree view
4. **TypeBrowserPanel.ui** - Type browser tree
5. **CallTreePanel.ui** - Function call tree
6. **StringsPanel.ui** - Strings table
7. **DataPanel.ui** - Global data table
8. **FunctionsPanel.ui** - Functions table
9. **EquatesPanel.ui** - Constants table

### Custom Widgets (`src/ui/widgets/`)

1. **DisassemblyTableWidget** - Custom table for disassembly
   - Address column (clickable for navigation)
   - Bytes column (hex display)
   - Stack depth column
   - Mnemonic column (syntax highlighted)
   - IL instruction column

2. **VBSyntaxHighlighter** - Syntax highlighter for VB6
   - Keywords (If, Then, Else, Function, Sub, etc.)
   - Comments (' or Rem)
   - Strings ("")
   - Numbers

3. **AddressLabel** - Clickable address labels
   - Cross-reference navigation
   - Context menu (Go to, Rename, etc.)

4. **HexViewWidget** - Hex/ASCII byte viewer
   - Traditional hex editor view
   - Synchronized with disassembly

## Data Flow

### Loading a VB Executable

```
User selects file
    ↓
PEFile::parse()
    ↓
VBFile::parse()
    ↓
Detect VB5! signature
    ↓
Parse VBHeader → ProjectInfo → ObjectTable
    ↓
For each object:
    ↓
    Extract method table
    ↓
    Disassemble methods (PCodeDisassembler or X86Disassembler)
    ↓
    Build symbol table (FUN_, DAT_)
    ↓
    Populate UI panels
```

### Decompiling a Function

```
User selects function in symbol browser
    ↓
Retrieve function bytes from PE sections
    ↓
Disassemble to P-Code or x86 instructions
    ↓
Lift to IR (IRFunction with IRBasicBlocks)
    ↓
Type inference (analyze operations and MSVBVM60 calls)
    ↓
Control flow structuring (If/Loop/Select)
    ↓
VBCodeGenerator::generate()
    ↓
Display in DecompilerPanel
```

## Performance Considerations

### Lazy Loading (Recommended)

- Parse PE headers immediately
- Parse VB header and object table on load
- Disassemble functions **on-demand** when viewed
- Cache disassembly results
- Background thread for full project analysis

### Caching Strategy

```cpp
class DisassemblyCache {
    std::unordered_map<uint32_t, std::vector<Instruction>> cache_;
    
public:
    std::optional<std::vector<Instruction>> get(uint32_t address);
    void put(uint32_t address, std::vector<Instruction> instructions);
};
```

### Background Processing

```cpp
class AsyncAnalyzer : public QThread {
    Q_OBJECT
    
signals:
    void progress(int percentage);
    void functionAnalyzed(const Function& func);
    void analysisComplete();
    
protected:
    void run() override;
};
```

## Build Configuration

### CMake Targets

```cmake
# Main executable
add_executable(vbdecompiler
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    resources/vbdecompiler.qrc
)

# Link Qt libraries
target_link_libraries(vbdecompiler PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# C++23 features
target_compile_features(vbdecompiler PRIVATE cxx_std_23)
```

### C++23 Features Used

- **Modules** (if compiler supports) - Faster compilation
- **Concepts** - Type constraints for templates
- **Ranges** - Elegant data processing
- **std::expected** - Better error handling than exceptions
- **std::span** - Safe array views
- **std::format** - Modern string formatting

## Testing Strategy

### Unit Tests

- PE parser tests (various PE files)
- VB header parsing tests
- P-Code instruction decoding tests
- Symbol table tests
- Type inference tests

### Integration Tests

- Load Phalanx.exe (real VB6 executable)
- Verify VB header detection
- Disassemble sample function
- Generate decompiled output
- Compare with expected results

### Test Framework

Use Qt Test framework:

```cpp
class TestPEParser : public QObject {
    Q_OBJECT
    
private slots:
    void testParseDOSHeader();
    void testParsePEHeader();
    void testGetSectionData();
};
```

## Future Enhancements

1. **VB Form Designer** - Visual reconstruction of VB forms
2. **Graph View** - Control flow graph visualization (like Ghidra)
3. **Scripting** - Python API for automation
4. **Binary Patching** - Modify and recompile VB binaries
5. **Debugger Integration** - Dynamic analysis capabilities
6. **Plugin System** - Extensible architecture
7. **Database Backend** - Store analysis in database for large projects

## References

- [Qt 6 Documentation](https://doc.qt.io/qt-6/)
- [PE Format Specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
- [VB_STRUCTURES.md](VB_STRUCTURES.md) - VB5/6 binary structures
- [Semi-VB-Decompiler](https://github.com/dzzie/SEMI_VBDecompiler) - VB research
- [Ghidra](https://ghidra-sre.org/) - UI inspiration
