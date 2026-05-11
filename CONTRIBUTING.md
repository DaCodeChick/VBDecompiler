# Contributing to VB Decompiler

Thank you for your interest in contributing to VB Decompiler!

## Development Principles

### 1. Cross-Platform First

**VB Decompiler is designed to run on Windows, Linux, and macOS.**

When writing code:

- ✅ **DO** use cross-platform APIs:
  - C standard library (stdio.h, stdlib.h, string.h)
  - Zig standard library cross-platform APIs
  - Qt 6 cross-platform APIs (in GUI code)
  
- ❌ **DON'T** use platform-specific APIs without guards:
  - POSIX APIs (unless wrapped with `@import("builtin").os.tag` checks)
  - Windows-specific APIs (unless wrapped with platform checks)
  - Linux-specific system calls

**Example of correct platform-specific code:**

```zig
const builtin = @import("builtin");

pub fn doSomething() void {
    switch (builtin.os.tag) {
        .windows => {
            // Windows-specific implementation
        },
        .linux, .macos => {
            // POSIX implementation
        },
        else => @compileError("Unsupported platform"),
    }
}
```

### 2. Memory Safety

- Use Zig's arena allocator for temporary allocations
- Always defer cleanup with `defer` or `errdefer`
- Avoid raw pointers when possible
- Use slices instead of pointer + length

### 3. Code Style

**Zig Code:**
- 4 spaces indentation
- camelCase for variables and functions
- PascalCase for types
- UPPER_CASE for constants

**C++ Code (Qt GUI):**
- Follow Qt coding conventions
- Use C++23 features where appropriate
- RAII for resource management

### 4. Testing

- Write unit tests for all public functions
- Run `zig build test` before committing
- Add integration tests for new features
- Test on multiple platforms if possible

### 5. Documentation

- Document all public APIs
- Update docs/ when adding features
- Keep README.md current
- Add inline comments for complex logic

## Pull Request Process

1. **Fork** the repository
2. **Create a branch** for your feature: `git checkout -b feature/my-feature`
3. **Make your changes** following the principles above
4. **Test thoroughly**: `zig build test && zig build`
5. **Commit** with clear messages: `git commit -m "Add feature X"`
6. **Push** to your fork: `git push origin feature/my-feature`
7. **Create a Pull Request** with:
   - Clear description of changes
   - Reference to any related issues
   - Screenshots for GUI changes
   - Test results from multiple platforms (if applicable)

## Code Review

All submissions require review. We use GitHub pull requests for this purpose.

Reviewers will check for:
- Cross-platform compatibility
- Memory safety
- Code style compliance
- Test coverage
- Documentation quality

## Reporting Issues

When reporting bugs:

1. **Search existing issues** first
2. **Provide system information**:
   - OS and version (Windows 11, Ubuntu 24.04, macOS 14, etc.)
   - Zig version (`zig version`)
   - Qt version (if GUI-related)
3. **Describe the problem**:
   - What you expected
   - What actually happened
   - Steps to reproduce
4. **Provide sample files** if possible (anonymized VB6 binaries)
5. **Include error messages** and stack traces

## Development Setup

### Prerequisites

- Zig 0.13.0+ (0.16.0 recommended)
- CMake 3.20+
- Qt 6.5+ (for GUI development)
- C++23 compiler (GCC 13+, Clang 16+, MSVC 2022+)

### Building

```bash
# Core library only
cd core
zig build

# Full project (requires Qt)
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Tests

```bash
cd core
zig build test
```

## Project Structure

```
VBDecompiler/
├── core/               # Zig decompiler core
│   ├── src/
│   │   ├── pe/         # PE parser
│   │   ├── vb6/        # VB6 detector
│   │   ├── disasm/     # Disassembler
│   │   ├── ir/         # IR layer
│   │   └── decompiler/ # Decompiler
│   └── tests/          # Unit tests
├── gui/                # Qt GUI
├── bindings/           # C API
└── docs/               # Documentation
```

## License

By contributing, you agree that your contributions will be licensed under the LGPL v3 license.

## Questions?

- Open an issue for questions
- Check existing documentation in `docs/`
- Review the architecture document: `docs/architecture.md`

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and grow
- Maintain professional communication

Thank you for contributing! 🎉
