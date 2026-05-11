# VB Decompiler Architecture

## Overview

The VB Decompiler is a comprehensive tool for reverse engineering Visual Basic 6 executables. It consists of two main components:

1. **Core Library (Zig)**: High-performance decompiler engine
2. **GUI Frontend (Qt 6/C++ 23)**: User-friendly interface similar to Ghidra/IDA

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│              Qt 6/C++ 23 GUI Frontend                   │
│  ┌──────────────────────────────────────────────────┐   │
│  │ Main Window                                      │   │
│  │ ┌──────────┬───────────────────────────────────┐ │   │
│  │ │ Function │ Disassembly / Decompiler View     │ │   │
│  │ │ Browser  ├───────────────────────────────────┤ │   │
│  │ │          │ Hex View                          │ │   │
│  │ └──────────┴───────────────────────────────────┘ │   │
│  └──────────────────────────────────────────────────┘   │
└───────────────────────┬─────────────────────────────────┘
                        │ C FFI
┌───────────────────────▼─────────────────────────────────┐
│           libvbdecomp.so/.dll/.dylib (Zig)              │
│  ┌──────────────────────────────────────────────────┐   │
│  │ C API Layer                                      │   │
│  ├──────────────────────────────────────────────────┤   │
│  │ PE Parser → VB6 Detector → Disassembler         │   │
│  │      ↓            ↓              ↓               │   │
│  │  Lifter → IR Optimizer → Decompiler             │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│              vbdecomp CLI (Zig)                         │
│  analyze | disasm | decompile | sections                │
└─────────────────────────────────────────────────────────┘
```

## Core Components

### 1. PE Parser (`core/src/pe/`)

Parses Windows Portable Executable files:
- DOS header and stub
- PE signature and headers
- COFF header
- Optional header (32-bit)
- Section headers (.text, .data, .rsrc, etc.)
- Data directories (imports, exports, resources)

### 2. VB6 Detector (`core/src/vb6/detector.zig`)

Identifies Visual Basic binaries:
- Checks for MSVBVM60.DLL/MSVBVM50.DLL imports
- Detects compilation type (Native vs P-Code)
- Identifies binary type (EXE, DLL, OCX)
- Locates VB object table
- Detects forms and resources

### 3. Disassembler (`core/src/disasm/`)

**Phase 1** (Current): Native x86 disassembly
- Custom x86 instruction decoder
- Linear sweep and recursive descent
- Basic block identification

**Phase 2**: P-Code disassembly
- P-Code opcode interpreter
- Stack simulation
- Conversion to readable format

### 4. IR Layer (`core/src/ir/`)

Intermediate representation for analysis:
- **Ghidra P-Code**: Primary IR (well-documented, proven)
  - SSA form
  - Generic operations
  - Architecture-independent
- **LLVM IR**: Secondary target for advanced optimizations

### 5. Decompiler (`core/src/decompiler/`)

High-level code generation:
- Type recovery (VB6 types: Integer, Long, String, Variant, Object, etc.)
- Control flow structuring (If/Then, Select Case, For/Next, Do/Loop)
- Variable naming
- Comment generation
- VB6 pseudo-code output

### 6. Analysis (`core/src/analysis/`)

Static analysis passes:
- **Cross-references**: Track code and data references
- **Strings**: Extract and catalog string literals
- **Functions**: Detect function boundaries and signatures
- **Data flow**: Track variable usage and propagation

## GUI Components

### Main Window (`gui/src/mainwindow.cpp`)

Central hub with dockable panels:
- Menu bar and toolbar
- Status bar
- Plugin architecture for extensibility

### Core Widgets

1. **Disassembly View** (`gui/src/widgets/disassembly_view.cpp`)
   - Address column
   - Bytes column (hex)
   - Instruction mnemonics
   - Operands with hyperlinks
   - Comments
   - Jump arrows for control flow

2. **Decompiler View** (`gui/src/widgets/decompiler_view.cpp`)
   - VB6 pseudo-code
   - Syntax highlighting
   - Collapsible code blocks
   - Side-by-side with disassembly

3. **Hex View** (`gui/src/widgets/hex_view.cpp`)
   - Address, hex, ASCII columns
   - Data type interpretation
   - Synchronized with other views

4. **Function Browser** (`gui/src/widgets/function_browser.cpp`)
   - Tree view of program structure
   - Modules, forms, classes
   - Functions and procedures
   - Search and filtering

5. **Graph View** (`gui/src/widgets/graph_view.cpp`)
   - Control flow graphs
   - Call graphs
   - Interactive navigation
   - Layout algorithms

6. **Cross-Reference View** (`gui/src/widgets/xref_view.cpp`)
   - "Where is this used?"
   - Code and data xrefs
   - Bidirectional navigation

7. **String/Resource Viewers**
   - String table
   - Icons, bitmaps, dialogs
   - Form data extraction

### DecompilerBridge (`gui/src/core/decompiler_bridge.cpp`)

Manages communication with Zig library:
- Dynamic library loading
- C API wrapping
- Error handling
- Thread safety

## Data Flow

### Analysis Pipeline

```
VB6 Binary (EXE/DLL/OCX)
    ↓
PE Parser
    ↓
VB6 Detector → Is VB? → Native or P-Code?
    ↓
Disassembler (x86 or P-Code)
    ↓
IR Lifter → Ghidra P-Code
    ↓
IR Optimizer → DCE, const prop, CSE
    ↓
Type Recovery → VB6 types
    ↓
Control Flow Structuring
    ↓
Code Generator → VB6 pseudo-code
    ↓
Display in GUI
```

### User Interaction Flow

```
User opens file in GUI
    ↓
GUI calls vbdecomp_open() via C API
    ↓
Core parses PE and detects VB6
    ↓
GUI displays file info
    ↓
User clicks on function in browser
    ↓
GUI calls vbdecomp_disassemble()
    ↓
Disassembly displayed in view
    ↓
User clicks "Decompile"
    ↓
GUI calls vbdecomp_decompile()
    ↓
VB6 pseudo-code displayed
```

## Design Decisions

### Why Zig for Core?

- **Performance**: Near C-level performance
- **Safety**: Compile-time memory safety checks
- **C interop**: Seamless C API generation
- **Modern**: Better ergonomics than C
- **No runtime**: Minimal dependencies

### Why Qt for GUI?

- **Cross-platform**: Linux, Windows, macOS
- **Mature**: Stable, well-documented
- **Widgets**: Rich set of UI components
- **C++ 23**: Modern C++ features
- **Community**: Large ecosystem

### Why Ghidra P-Code?

- **Proven**: Used in production decompiler
- **Documented**: Extensive documentation
- **Generic**: Architecture-independent
- **Simple**: Easier to implement than LLVM IR
- **Extensible**: Can add custom operations

### Why Custom x86 Disassembler?

- **Control**: Full control over output format
- **Learning**: Educational value
- **VB6-specific**: Can add VB6-specific annotations
- **No dependencies**: Reduces complexity
- **Lightweight**: Smaller binary size

## Project Database Format

Analysis results stored in **SQLite database**:

### Rationale
- **Efficient**: Fast queries for large binaries
- **Standard**: Well-supported, portable
- **Structured**: Relational model fits our needs
- **Extensible**: Easy to add new tables
- **Tooling**: Can inspect with standard SQL tools

### Schema (simplified)

```sql
-- Binary metadata
CREATE TABLE binary_info (
    path TEXT PRIMARY KEY,
    vb_version INTEGER,
    binary_type INTEGER,
    compilation_type INTEGER,
    entry_point INTEGER,
    image_base INTEGER
);

-- Functions
CREATE TABLE functions (
    address INTEGER PRIMARY KEY,
    name TEXT,
    size INTEGER,
    is_export BOOLEAN,
    is_thunk BOOLEAN
);

-- Disassembly
CREATE TABLE disassembly (
    address INTEGER PRIMARY KEY,
    mnemonic TEXT,
    operands TEXT,
    bytes BLOB,
    comment TEXT
);

-- User annotations
CREATE TABLE comments (
    address INTEGER PRIMARY KEY,
    text TEXT,
    author TEXT,
    timestamp INTEGER
);

-- Cross-references
CREATE TABLE xrefs (
    from_addr INTEGER,
    to_addr INTEGER,
    type INTEGER,
    PRIMARY KEY (from_addr, to_addr)
);
```

## Build System

### CMake Integration

1. CMake invokes `zig build` for core library
2. CMake builds Qt GUI
3. CMake links GUI against libvbdecomp
4. CMake creates installer package

### Build Targets

- `vbdecomp_core`: Zig shared library
- `vbdecomp-gui`: Qt application
- `vbdecomp`: CLI tool (built by Zig)
- `install`: Install all components
- `test`: Run unit tests

## Testing Strategy

### Unit Tests (Zig)

```bash
cd core
zig build test
```

Tests each module independently.

### Integration Tests

End-to-end tests with sample VB6 binaries:
- Simple Hello World (Native + P-Code)
- GUI app with forms
- DLL with exports
- OCX control

### GUI Tests (Qt Test)

```bash
cd gui/tests
./test_runner
```

Tests UI components and interactions.

## Extension Points

1. **Custom IR passes**: Add optimization passes
2. **Analysis plugins**: New analysis algorithms
3. **GUI plugins**: Custom views and tools
4. **Export formats**: HTML, Markdown, JSON
5. **Scripting**: Python/Lua bindings (future)

## Performance Considerations

- **Lazy loading**: Parse sections on-demand
- **Caching**: Cache disassembly and decompilation results
- **Threading**: Background analysis in worker threads
- **Streaming**: Process large files incrementally
- **Memory mapping**: Use mmap for file access

## Security

- **Sandboxing**: Parse untrusted binaries safely
- **Input validation**: Validate all PE structures
- **Bounds checking**: Prevent buffer overflows
- **No code execution**: Never execute target binary
- **Fuzzing**: Test with malformed inputs

## Future Enhancements

1. **P-Code decompilation**: Complete P-Code support
2. **Form reconstruction**: Visual form editor
3. **Scripting**: Python API for automation
4. **Collaborative**: Multi-user annotations
5. **Cloud**: Save projects to cloud storage
6. **Diff**: Compare different versions
7. **Signature matching**: Library function identification
8. **Type libraries**: Import .tlb files for better typing
