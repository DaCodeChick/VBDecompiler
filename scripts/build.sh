#!/bin/bash
# Build and test script for VB Decompiler (Linux/macOS)

set -e  # Exit on error

echo "================================================"
echo "  VB Decompiler - Build and Test"
echo "================================================"
echo ""

# Check prerequisites
echo "[1/5] Checking prerequisites..."
if ! command -v zig &> /dev/null; then
    echo "❌ Error: Zig compiler not found"
    echo "   Install from: https://ziglang.org/download/"
    exit 1
fi

ZIG_VERSION=$(zig version)
echo "✅ Zig version: $ZIG_VERSION"

if ! command -v cmake &> /dev/null; then
    echo "⚠️  Warning: CMake not found (required for full build with GUI)"
else
    CMAKE_VERSION=$(cmake --version | head -n1)
    echo "✅ $CMAKE_VERSION"
fi

echo ""

# Build core library
echo "[2/5] Building Zig core library..."
cd core
zig build -Doptimize=ReleaseSafe
cd ..
echo "✅ Core library built successfully"
echo ""

# Run tests
echo "[3/5] Running unit tests..."
cd core
if zig build test 2>&1 | grep -q "All [0-9]* tests passed"; then
    echo "✅ All tests passed"
else
    echo "⚠️  Some tests failed"
fi
cd ..
echo ""

# Verify artifacts
echo "[4/5] Verifying build artifacts..."
if [ -f "core/zig-out/lib/libvbdecomp.so" ] || [ -f "core/zig-out/lib/libvbdecomp.dylib" ]; then
    LIB_SIZE=$(du -h core/zig-out/lib/libvbdecomp.* | cut -f1)
    echo "✅ Shared library: $LIB_SIZE"
else
    echo "❌ Shared library not found"
    exit 1
fi

if [ -f "core/zig-out/bin/vbdecomp" ]; then
    CLI_SIZE=$(du -h core/zig-out/bin/vbdecomp | cut -f1)
    echo "✅ CLI tool: $CLI_SIZE"
else
    echo "❌ CLI tool not found"
    exit 1
fi
echo ""

# Test CLI
echo "[5/5] Testing CLI tool..."
OUTPUT=$(./core/zig-out/bin/vbdecomp)
if echo "$OUTPUT" | grep -q "VBDecompiler CLI"; then
    echo "✅ CLI tool works"
else
    echo "❌ CLI tool failed"
    exit 1
fi
echo ""

echo "================================================"
echo "  ✅ Build completed successfully!"
echo "================================================"
echo ""
echo "Artifacts:"
echo "  - Library: core/zig-out/lib/"
echo "  - CLI:     core/zig-out/bin/vbdecomp"
echo ""
echo "Run CLI: ./core/zig-out/bin/vbdecomp"
