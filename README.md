# VBDecompiler

A Ghidra-style decompiler for Visual Basic 5/6 executables, built with C++23 and Qt 6.

## Features

- **Multi-format Support**: Analyze .exe, .dll, and .ocx VB binaries
- **Dual Disassembly**: View both P-Code (VB intermediate language) and x86 native code
- **Ghidra-style UI**: Familiar workspace with symbol browser, type browser, and function call trees
- **Smart Symbol Naming**: Automatic `FUN_` and `DAT_` naming conventions
- **VB6 Decompilation**: Reconstruct original Visual Basic 6 source code
- **Comprehensive Analysis**:
  - Disassembly listing with bytes, stack depth, mnemonic, and IL instruction columns
  - Defined strings browser
  - Global data viewer
  - Function table with cross-references
  - Equates and constants

## Architecture

### Technology Stack

- **Language**: C++23
- **UI Framework**: Qt 6 (Widgets, Core, Gui)
- **UI Design**: `.ui` files compiled with Qt's `uic` meta-compiler
- **Build System**: CMake 3.20+
- **Target Platforms**: Windows, Linux, macOS

### Project Structure

```
VBDecompiler/
├── CMakeLists.txt              # Build configuration
├── src/
│   ├── main.cpp                # Application entry point
│   ├── core/                   # Core decompiler engine
│   │   ├── pe/                 # PE file format parser
│   │   ├── vb/                 # VB5/6 binary format parser
│   │   ├── disasm/             # P-Code and x86 disassemblers
│   │   ├── ir/                 # Intermediate representation
│   │   ├── decompiler/         # Decompilation engine
│   │   └── symbols/            # Symbol table management
│   ├── ui/                     # Qt UI components
│   │   ├── MainWindow.{h,cpp,ui}
│   │   ├── panels/             # UI panels (.ui files)
│   │   └── widgets/            # Custom Qt widgets
│   └── utils/                  # Utilities
├── include/                    # Public headers
├── resources/                  # Qt resources (icons, etc.)
├── tests/                      # Unit tests
└── docs/                       # Documentation
```

## Building

### Prerequisites

- **CMake** 3.20 or later
- **Qt 6.5+** (Core, Widgets, Gui modules)
- **C++23 compatible compiler**:
  - GCC 13+
  - Clang 16+
  - MSVC 2022 (17.6+)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/VBDecompiler.git
cd VBDecompiler

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Run
./bin/vbdecompiler
```

### Linux/macOS

```bash
# Install Qt 6
# Ubuntu/Debian:
sudo apt install qt6-base-dev cmake g++-13

# macOS (Homebrew):
brew install qt@6 cmake

# Build
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=g++-13 ..
make -j$(nproc)
```

### Windows

```bash
# Install Qt 6 from https://www.qt.io/download
# Install Visual Studio 2022 with C++ workload

# Build using Visual Studio Developer Command Prompt
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## Usage

### Opening a VB Executable

```bash
vbdecompiler /path/to/executable.exe
```

### UI Overview

The VBDecompiler workspace consists of:

**Left Panel:**
- **Symbol Tree**: Navigate functions, data, and strings
- **Type Manager**: Browse VB types and structures

**Center Panel (Tabbed):**
- **Disassembly Listing**: Main disassembly view with address, bytes, stack depth, mnemonic, and IL instruction columns
- **Decompiler View**: Reconstructed VB6 source code

**Right Panel:**
- **Function Call Tree**: Incoming/outgoing calls hierarchy (Ghidra-style)
- **References Panel**: Cross-references (xrefs)

**Bottom Tabs:**
- **Defined Strings**: String literals table
- **Data**: Global variables listing
- **Functions**: Function table with addresses
- **Equates**: Constants and enums

### Switching Between P-Code and x86 Views

Use the **View** menu or toolbar button to toggle between P-Code (VB intermediate language) and x86 native assembly views.

## VB Binary Support

### Supported Dialects
- Visual Basic 5 (VB5)
- Visual Basic 6 (VB6)

### Compilation Modes
- **P-Code**: VB bytecode executed by MSVBVM60.DLL runtime
- **Native Code**: Compiled to x86 machine code

### File Formats
- `.exe` - VB executables
- `.dll` - VB DLLs
- `.ocx` - ActiveX controls

### Detection
The decompiler automatically detects:
- VB5/VB6 signature (`VB5!` header)
- P-Code vs native compilation
- UPX compression (requires external `upx` tool)

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture and design
- [VB_STRUCTURES.md](VB_STRUCTURES.md) - VB5/6 binary format reference
- [PCODE_REFERENCE.md](docs/PCODE_REFERENCE.md) - P-Code instruction set

## Research References

Based on research from:
- [Semi-VB-Decompiler](https://github.com/dzzie/SEMI_VBDecompiler) - VB structure definitions
- Various VB reverse engineering resources

## Contributing

Contributions welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

GPL-3.0 License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- Inspired by [Ghidra](https://ghidra-sre.org/) reverse engineering framework
- VB structure definitions from Semi-VB-Decompiler project
- VB community reverse engineering research
