# Building VB Decompiler

## Prerequisites

### Required Tools

- **Zig** 0.13.0 or later
  - Download from: https://ziglang.org/download/
  - Add to PATH

- **CMake** 3.20 or later
  - Linux: `sudo apt install cmake` (Ubuntu/Debian)
  - macOS: `brew install cmake`
  - Windows: Download from https://cmake.org/

- **Qt 6** 6.5 or later
  - Linux: `sudo apt install qt6-base-dev` (Ubuntu/Debian)
  - macOS: `brew install qt@6`
  - Windows: Download from https://www.qt.io/download

- **C++ Compiler** with C++23 support
  - Linux: GCC 13+ or Clang 16+
  - macOS: Xcode 15+ or Clang 16+
  - Windows: Visual Studio 2022 or later

## Platform-Specific Instructions

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake qt6-base-dev git

# Install Zig
wget https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz
tar -xf zig-linux-x86_64-0.13.0.tar.xz
sudo mv zig-linux-x86_64-0.13.0 /opt/zig
echo 'export PATH=/opt/zig:$PATH' >> ~/.bashrc
source ~/.bashrc

# Clone and build
git clone https://github.com/yourusername/VBDecompiler.git
cd VBDecompiler

# Build core library
cd core
zig build -Doptimize=ReleaseSafe
cd ..

# Build full project with CMake
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)

# Install
sudo cmake --install .
```

### macOS

```bash
# Install dependencies
brew install cmake qt@6 zig

# Clone and build
git clone https://github.com/yourusername/VBDecompiler.git
cd VBDecompiler

# Build core library
cd core
zig build -Doptimize=ReleaseSafe
cd ..

# Build with CMake
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) ..
cmake --build . -j$(sysctl -n hw.ncpu)

# Install
sudo cmake --install .
```

### Windows (Visual Studio)

```powershell
# Prerequisites:
# 1. Install Visual Studio 2022 with C++ workload
# 2. Install CMake from https://cmake.org/
# 3. Install Qt 6 from https://www.qt.io/download
# 4. Install Zig from https://ziglang.org/download/

# Add Zig to PATH
$env:PATH += ";C:\zig"

# Clone repository
git clone https://github.com/yourusername/VBDecompiler.git
cd VBDecompiler

# Build core library
cd core
zig build -Doptimize=ReleaseSafe
cd ..

# Build with CMake
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A Win32 `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.5.0\msvc2022" ..
cmake --build . --config Release

# Install
cmake --install . --config Release
```

## Build Options

### CMake Options

```bash
cmake -DBUILD_GUI=ON \         # Build Qt GUI (default: ON)
      -DBUILD_CLI=ON \         # Build CLI tool (default: ON)
      -DBUILD_TESTS=ON \       # Build tests (default: ON)
      -DCMAKE_BUILD_TYPE=Release ..
```

### Zig Build Options

```bash
# In core/ directory
zig build -Doptimize=Debug          # Debug build
zig build -Doptimize=ReleaseSafe    # Release with safety checks
zig build -Doptimize=ReleaseFast    # Maximum optimization
zig build -Doptimize=ReleaseSmall   # Optimize for size
```

## Building Only Core Library

If you only want the core decompiler library without the GUI:

```bash
cd core
zig build -Doptimize=ReleaseSafe

# Library will be in: core/zig-out/lib/libvbdecomp.so (Linux)
#                     core/zig-out/lib/libvbdecomp.dylib (macOS)
#                     core/zig-out/lib/vbdecomp.dll (Windows)

# CLI tool will be in: core/zig-out/bin/vbdecomp
```

## Building Only CLI Tool

```bash
cd core
zig build -Doptimize=ReleaseSafe

# Run CLI
./zig-out/bin/vbdecomp help
```

## Running Tests

### Zig Unit Tests

```bash
cd core
zig build test
```

### Integration Tests

```bash
cd build
ctest
```

## Development Build

For active development with faster compile times:

```bash
# Debug build with all checks
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .

# Run with debugger
gdb ./gui/vbdecomp-gui
# or
lldb ./gui/vbdecomp-gui
```

## Cross-Compilation

### Linux → Windows

```bash
cd core
zig build -Dtarget=x86-windows-gnu -Doptimize=ReleaseSafe

# This creates vbdecomp.dll for Windows
```

### macOS → Linux

```bash
cd core
zig build -Dtarget=x86_64-linux-gnu -Doptimize=ReleaseSafe
```

## Troubleshooting

### "Zig not found"

Make sure Zig is in your PATH:
```bash
which zig  # Linux/macOS
where zig  # Windows
```

### "Qt not found"

Specify Qt location explicitly:
```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/qt6 ..
```

### "C++23 not supported"

Update your compiler:
- Linux: `sudo apt install g++-13` or `sudo apt install clang-16`
- macOS: Update Xcode Command Line Tools
- Windows: Use Visual Studio 2022 17.6 or later

### Zig Build Errors

Clear cache and rebuild:
```bash
cd core
rm -rf zig-cache zig-out
zig build
```

### CMake Cache Issues

```bash
cd build
rm -rf *
cmake ..
```

## IDE Setup

### Visual Studio Code

Install extensions:
- C/C++ (Microsoft)
- CMake Tools
- Zig Language

Open project and use CMake extension to configure and build.

### CLion

Open CMakeLists.txt as project. CLion will auto-configure.

### Qt Creator

Open CMakeLists.txt. Qt Creator will detect Qt automatically.

## Next Steps

After building:

1. **Try the CLI tool**:
   ```bash
   ./core/zig-out/bin/vbdecomp analyze <vb6-file.exe>
   ```

2. **Launch the GUI** (once implemented):
   ```bash
   ./build/gui/vbdecomp-gui
   ```

3. **Read the documentation**:
   - [Architecture](architecture.md)
   - [C API Reference](api.md)
   - [VB6 Format](vb6-format.md)
