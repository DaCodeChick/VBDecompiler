# VBDecompiler

A Ghidra-style decompiler for Visual Basic 5/6 executables, built with a **Rust decompilation engine** and **Qt 6 GUI**.

## Features

- **Multi-format Support**: Analyze .exe, .dll, and .ocx VB binaries
- **Dual Disassembly**: View both P-Code (VB intermediate language) and x86 native code
- **Packer Detection**: Automatic detection of UPX, ASPack, PECompact, and other common packers
- **Ghidra-style UI**: Familiar workspace with symbol browser, type browser, and function call trees
- **Smart Symbol Naming**: Automatic `FUN_` and `DAT_` naming conventions
- **VB6 Decompilation**: Reconstruct original Visual Basic 6 source code
- **Memory-Safe Core**: Rust implementation for reliable, crash-free decompilation
- **Parallel Decompilation**: Multi-threaded processing with Rayon scales to all CPU cores
- **Comprehensive Analysis**:
  - Disassembly listing with bytes, stack depth, mnemonic, and IL instruction columns
  - Defined strings browser
  - Global data viewer
  - Function table with cross-references
  - Equates and constants

## Architecture

### Technology Stack

- **Core Engine**: Rust 2021 (memory-safe decompilation logic with parallel processing)
- **GUI Framework**: C++23 + Qt 6 (Widgets, Core, Gui)
- **FFI Layer**: C bindings bridging Rust core to C++ GUI
- **Build System**: CMake 3.20+ (C++/Qt) + Cargo (Rust)
- **Disassembly**: iced-x86 for professional x86/x64 disassembly
- **Packer Detection**: Entropy analysis and signature-based detection (goblin + entropy crate)
- **Concurrency**: Rayon for data-parallel method decompilation
- **Target Platforms**: Windows, Linux, macOS

### Hybrid Architecture

VBDecompiler uses a unique hybrid architecture combining Rust's safety with Qt's mature GUI:

```
┌─────────────────────────────────────────┐
│         Qt6 GUI (C++)                   │
│    - MainWindow and UI components       │
└──────────────────┬──────────────────────┘
                   │ C FFI
                   ↓
┌─────────────────────────────────────────┐
│      Rust Decompilation Core            │
│  ┌───────────────────────────────────┐  │
│  │  vbdecompiler-ffi (C bindings)    │  │
│  └──────────────┬────────────────────┘  │
│                 ↓                        │
│  ┌───────────────────────────────────┐  │
│  │  vbdecompiler-core                │  │
│  │  - PE parsing (goblin)            │  │
│  │  - Packer detection (entropy)     │  │
│  │  - VB5/6 structure parsing        │  │
│  │  - P-Code disassembler            │  │
│  │  - x86 disassembler (iced-x86)    │  │
│  │  - IR system                      │  │
│  │  - P-Code to IR lifter            │  │
│  │  - VB6 code generator             │  │
│  └───────────────────────────────────┘  │
│                                          │
│  ┌───────────────────────────────────┐  │
│  │  vbdecompiler-cli (optional)      │  │
│  │  - Standalone command-line tool   │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

### Project Structure

```
VBDecompiler/
├── CMakeLists.txt              # C++ build configuration
├── Cargo.toml                  # Rust workspace configuration
├── crates/                     # Rust crates
│   ├── vbdecompiler-core/      # Core decompilation engine
│   ├── vbdecompiler-cli/       # Command-line tool
│   └── vbdecompiler-ffi/       # C FFI bindings
├── src/
│   ├── main.cpp                # Application entry point
│   └── ui/                     # Qt UI components
│       └── MainWindow.{h,cpp,ui}
├── include/
│   └── vbdecompiler_ffi.h      # C FFI header
├── tests/                      # C++ unit tests
└── docs/                       # Documentation
```

## Building

### Prerequisites

- **Rust** 1.70+ (`cargo`, `rustc`)
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

# Rust builds automatically during CMake build
mkdir build && cd build

# Configure with CMake (builds Rust library automatically)
cmake ..

# Build
cmake --build .

# Run GUI
./bin/vbdecompiler

# Or use the CLI tool directly
cargo run --bin vbdecompiler-cli -- decompile <file.exe>
```

### Linux/macOS

```bash
# Install dependencies
# Ubuntu/Debian:
sudo apt install qt6-base-dev cmake g++-13 cargo

# Arch Linux:
sudo pacman -S qt6-base cmake gcc rust

# macOS (Homebrew):
brew install qt@6 cmake rust

# Build
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=g++-13 ..
make -j$(nproc)
```

### Windows

```bash
# Install Rust from https://rustup.rs/
# Install Qt 6 from https://www.qt.io/download
# Install Visual Studio 2022 with C++ workload

# Build using Visual Studio Developer Command Prompt
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## Usage

### GUI Application

Launch the Qt6 GUI application:

```bash
# From build directory
./bin/vbdecompiler

# Or from install location
vbdecompiler
```

**Opening a File:**
1. Click **File → Open** or use the toolbar button
2. Select a VB5/6 executable (.exe, .dll, or .ocx)
3. The decompiler will parse the binary and populate the UI

**UI Overview:**

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

**Switching Between P-Code and x86 Views:**

Use the **View** menu or toolbar button to toggle between P-Code (VB intermediate language) and x86 native assembly views.

### Command-Line Tool

For batch processing or scripting, use the Rust CLI:

```bash
# Decompile a VB executable
cargo run --bin vbdecompiler-cli -- decompile input.exe

# Output to file
cargo run --bin vbdecompiler-cli -- decompile input.exe --output output.vb

# With detailed logging
RUST_LOG=debug cargo run --bin vbdecompiler-cli -- decompile input.exe
```

### Programmatic API (Rust)

Add to your `Cargo.toml`:

```toml
[dependencies]
vbdecompiler-core = { path = "./crates/vbdecompiler-core" }
```

Example usage:

```rust
use vbdecompiler_core::{Decompiler, DecompilerOptions};
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = DecompilerOptions::default();
    let decompiler = Decompiler::new(options);
    let result = decompiler.decompile_file(Path::new("input.exe"))?;
    
    println!("Decompiled {} methods", result.methods.len());
    for method in result.methods {
        println!("Method at 0x{:08x}:\n{}", method.address, method.code);
    }
    Ok(())
}
```

## Testing

### Rust Core Tests

The Rust decompilation core has comprehensive unit tests:

```bash
# Run all Rust tests
cargo test --all

# Run with output
cargo test --all -- --nocapture

# Run specific crate tests
cargo test -p vbdecompiler-core

# With coverage (requires cargo-tarpaulin)
cargo tarpaulin --all --out Html
```

### C++ Tests

C++ tests focus on the X86 disassembler component:

```bash
# Build tests
cd build
cmake --build . --target test_x86

# Run tests
./bin/test_x86
```

### Integration Testing

To test the full GUI → FFI → Rust pipeline:

1. Build the complete project:
   ```bash
   mkdir build && cd build
   cmake .. && cmake --build .
   ```

2. Run the GUI:
   ```bash
   ./bin/vbdecompiler
   ```

3. Load a test VB executable (P-Code compiled VB5/6 binary)

4. Verify:
   - No crashes during file loading
   - Decompiled output appears in the center panel
   - Symbol tree populates with discovered methods
   - Log output shows parsing progress

### Sample Test Files

The project includes sample VB executables for testing:

- `tests/samples/simple_pcode.exe` - Basic P-Code executable
- `tests/samples/forms_app.exe` - VB app with forms
- `tests/samples/native_code.exe` - Native-compiled VB (x86 only)

Note: These are not included in the repository due to size. You can create test binaries using Visual Basic 6.0 IDE with "Compile to P-Code" option.

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
- Packed/compressed executables:
  - **UPX** (Ultimate Packer for eXecutables)
  - **ASPack** (commercial packer)
  - **PECompact** (commercial packer)
  - **Themida/WinLicense** (advanced protection)
  - **FSG, Petite, MEW, NSPack** (other common packers)
  - **Unknown packers** via entropy analysis

### Packed Executables

If the decompiler detects a packed executable, it will display an error message with unpacking instructions. For UPX-packed files:

```bash
# Install UPX from https://upx.github.io/
# Unpack the executable
upx -d packed.exe -o unpacked.exe

# Then decompile the unpacked version
./vbdecompiler unpacked.exe
```

The packer detection system uses:
- **Section name analysis**: Identifies packer signatures (e.g., "UPX0", ".aspack")
- **Entropy analysis**: Detects compressed/encrypted sections (Shannon entropy > 7.2)
- **Import table analysis**: Flags suspiciously minimal imports

For other packers, consult the specific unpacker tool or use universal unpackers.

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
