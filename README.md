# VB Decompiler

A modern, open-source decompiler for Visual Basic 6 executables (EXE, DLL, OCX) with a Ghidra/IDA-like GUI.

## Features

- **Cross-platform**: Works on Windows, Linux, and macOS
- **Multi-format support**: Native code (x86), P-Code, DLL, and OCX files
- **Multi-level analysis**: Disassembly → Intermediate Representation → High-level VB6 code
- **Advanced GUI**: Qt 6-based interface with:
  - Disassembly viewer with syntax highlighting
  - Decompiled code viewer
  - Hex viewer
  - Function/Form browser
  - Cross-reference analysis
  - String and resource extraction
  - Control flow graph visualization
- **Fast core**: Written in Zig for performance and safety
- **Extensible**: C API for integration with other tools

## Architecture

```
┌─────────────────────────────────────────────────┐
│           Qt 6/C++ 23 GUI Frontend              │
│  (Ghidra/IDA-like interface with all features)  │
└───────────────┬─────────────────────────────────┘
                │ C API (FFI)
┌───────────────▼─────────────────────────────────┐
│          libvbdecomp.so/.dll/.dylib             │
│         (Zig Shared Library - Core)             │
└───────────────┬─────────────────────────────────┘
                │
┌───────────────▼─────────────────────────────────┐
│          vbdecomp CLI Tool (Zig)                │
└─────────────────────────────────────────────────┘
```

## Building

### Prerequisites

- **Zig** 0.13.0 or later
- **CMake** 3.20 or later
- **Qt 6** 6.5 or later
- **C++ compiler** with C++23 support (GCC 13+, Clang 16+, MSVC 2022+)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/VBDecompiler.git
cd VBDecompiler

# Build the Zig core
cd core
zig build -Doptimize=ReleaseSafe
cd ..

# Build with CMake (includes GUI)
mkdir build && cd build
cmake ..
cmake --build .

# Or use the build script
./scripts/build.sh
```

### Build Options

- `BUILD_GUI=ON/OFF` - Build Qt GUI (default: ON)
- `BUILD_CLI=ON/OFF` - Build CLI tool (default: ON)
- `BUILD_TESTS=ON/OFF` - Build tests (default: ON)

## Usage

### CLI Tool

```bash
# Analyze a VB6 executable
./vbdecomp analyze program.exe

# Disassemble a function
./vbdecomp disasm program.exe --address 0x401000

# Decompile a function
./vbdecomp decompile program.exe --address 0x401000

# Extract resources
./vbdecomp extract program.exe --output ./resources/
```

### GUI

```bash
# Launch the GUI
./vbdecomp-gui

# Open a file directly
./vbdecomp-gui program.exe
```

## Project Status

**Current Phase**: Phase 1 - Foundation

- [x] Project structure
- [x] Build system setup
- [ ] PE parser implementation
- [ ] VB6 detector
- [ ] Basic CLI tool
- [ ] C API skeleton

See [docs/architecture.md](docs/architecture.md) for detailed architecture documentation.

## Documentation

- [Architecture Overview](docs/architecture.md)
- [VB6 Binary Format](docs/vb6-format.md)
- [C API Reference](docs/api.md)
- [Build Instructions](docs/building.md)

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This project is licensed under the GNU Lesser General Public License v3.0 (LGPL-3.0).
See [LICENSE](LICENSE) for details.

## Acknowledgments

- VB6 format research from the reverse engineering community
- Inspiration from Ghidra, IDA Pro, and other decompiler projects
- P-Code and LLVM IR concepts for intermediate representation

## Disclaimer

This tool is intended for legitimate reverse engineering purposes such as:
- Security research and vulnerability analysis
- Software archaeology and preservation
- Interoperability and compatibility work
- Educational purposes

Users are responsible for ensuring their use complies with applicable laws and licenses.
