# Agent Knowledge Base for VBDecompiler

This document contains critical information for AI agents working on the VBDecompiler project. Reading this file will save significant time and prevent common pitfalls.

## Project Overview

**VBDecompiler** is a cross-platform VB6 decompiler written in Zig 0.16.0, targeting native code analysis with CFG, P-code IR, and data-flow analysis.

- **Language**: Zig 0.16.0
- **License**: LGPL v3
- **Architecture**: Core library (Zig) + C API + CLI + future Qt GUI
- **Build System**: Zig build (`core/build.zig`) + CMake (root)
- **Target**: VB6 native code (PE executables, DLLs, OCX)

## CRITICAL: Zig 0.16 API Gotchas

### ArrayList (Unmanaged) - MOST COMMON ISSUE

Zig 0.16 has TWO ArrayList types:
1. **`std.ArrayList(T)`** → Maps to `array_list.Aligned` (UNMANAGED - no allocator field)
2. **`std.ArrayListManaged(T)`** → Has allocator field

We use the **unmanaged** version throughout this project. Here's the correct usage:

#### ✅ CORRECT Usage

```zig
// Initialization
var list: std.ArrayList(u32) = .empty;
defer list.deinit(allocator);

// Append
try list.append(allocator, value);

// Access
for (list.items) |item| { ... }

// Pop (returns optional!)
const item = list.pop() orelse unreachable;  // or use if/while
```

#### ❌ WRONG Usage (Will Not Compile)

```zig
// DON'T DO THIS - .init() doesn't exist for unmanaged
var list = std.ArrayList(u32).init(allocator);

// DON'T DO THIS - old struct literal syntax
var list: std.ArrayList(u32) = .{ .items = &.{}, .capacity = 0 };

// DON'T DO THIS - missing allocator in deinit
defer list.deinit();

// DON'T DO THIS - missing allocator in append
try list.append(value);

// DON'T DO THIS - pop() returns optional!
const item = list.pop();  // Type error if you expect non-optional
```

#### Important Details

- **`pop()` returns `?T`** (optional) in Zig 0.16, even for unmanaged ArrayList
- Always unwrap with `orelse`, `.?`, or `if` statement
- Use `while (list.items.len > 0)` check, then `list.pop() orelse unreachable` is safe

#### Migration Pattern

When you see old-style ArrayList code, replace it:

```zig
// OLD (Zig < 0.13)
var list = std.ArrayList(u32).init(allocator);

// NEW (Zig 0.16)
var list: std.ArrayList(u32) = .empty;
```

### AutoHashMap

AutoHashMap works normally - no special changes needed:

```zig
var map = std.AutoHashMap(u32, Value).init(allocator);
defer map.deinit();

try map.put(key, value);
const val = map.get(key);
```

### std.Io Interface (File I/O)

Zig 0.16 uses `std.Io` interface pattern:

```zig
var threaded = std.Io.Threaded.init(allocator, .{});
const io = threaded.io();

const stdout_file = std.Io.File.stdout();
var write_buffer: [8192]u8 = undefined;
var writer = stdout_file.writer(io, &write_buffer);

try writer.interface.print("Hello {s}\n", .{"world"});
try writer.flush();
```

Don't use old `std.fs.File.writer()` pattern.

#### Simpler Pattern for stdout (Zig 0.16+)

For simple console output, you can use `writeStreamingAll`:

```zig
pub fn main(init: std.process.Init) !void {
    try std.Io.File.stdout().writeStreamingAll(init.io, "Hello, World!\n");
}
```

**Note**: This requires main to use `std.process.Init` signature. Our current implementation uses the C-style `main(argc, argv)` for simplicity, but `std.process.Init` is the idiomatic Zig 0.16 approach.

## C FFI Status

### ~~2. C stdlib Import (Design Choice)~~ ❌ REMOVED

**Previous Status**: The CLI and parser used C stdlib for I/O (`c.printf`, `c.fopen`, etc.)

**Current Status**: **ALL C stdlib imports have been removed** as of commit `487dc86`
- Replaced `c.printf()` with custom `print()` helper using `std.Io`
- Replaced `c.fopen()/fread()/fclose()` with `std.Io.Dir` and `std.Io.File` APIs
- All format strings converted from C (`%s`, `%d`, `%X`) to Zig (`{s}`, `{}`, `{X}`)
- **Zero C FFI dependencies** in core codebase (except for intended C API export)
- Only use C API export (`lib.zig`) for the Qt GUI interface

## Project Structure

```
VBDecompiler/
├── core/               # Zig implementation
│   ├── src/
│   │   ├── main.zig           # CLI entry point
│   │   ├── lib.zig            # C API exports
│   │   ├── pe/                # PE parser
│   │   ├── vb6/               # VB6 detection
│   │   ├── disasm/            # x86 disassembler
│   │   ├── analysis/          # CFG, dominators, dataflow, type inference
│   │   ├── ir/                # P-code IR, SSA
│   │   ├── decompiler/        # High-level code generation
│   │   ├── db/                # SQLite project database
│   │   └── utils/             # Errors, logging
│   └── build.zig              # Zig build config
├── bindings/
│   └── include/vbdecomp.h     # C API header
├── gui/                       # Future Qt GUI
├── docs/                      # Architecture docs
└── CMakeLists.txt             # Root build
```

## Code Architecture

### Data Structures

#### CFG (Control Flow Graph)
- Located: `core/src/analysis/cfg.zig`
- Key types: `CFG`, `BasicBlock`, `Edge`, `XRef`
- **IMPORTANT**: `Edge` has fields `.from`, `.to`, `.type` (NOT `.target`!)

```zig
// CORRECT
for (block.successors) |edge| {
    const target = edge.to;  // ✅
}

// WRONG
for (block.successors) |edge| {
    const target = edge.target;  // ❌ Field doesn't exist!
}
```

#### P-code IR
- Located: `core/src/ir/`
- Ghidra-style intermediate representation
- Varnodes represent storage locations
- Operations are architecture-neutral

#### Data Flow Analysis
- **Reaching Definitions**: `core/src/analysis/reaching_defs.zig`
- **Liveness**: `core/src/analysis/liveness.zig`
- **Dominators**: `core/src/analysis/dominators.zig`

### Memory Management

- **Primary allocator**: Arena allocator over page allocator
- **Pattern**: Create arena in main, pass allocator down
- **Cleanup**: `defer arena.deinit()` at top level

```zig
var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
defer arena.deinit();
const allocator = arena.allocator();
```

## CLI Commands (Current Status)

Build: `cd core && zig build`

Available commands:
- `analyze <file>` - Analyze VB6 binary
- `sections <file>` - List PE sections
- `disasm <file> <addr>` - Disassemble at address
- `cfg <file> <addr>` - Control flow analysis
- `xrefs <file> <addr>` - Cross-references
- `blocks <file> <addr>` - List basic blocks
- `dot <file> <addr> [out]` - Export CFG to DOT
- `pcode <file> <addr>` - Generate P-code IR
- `dataflow <file> <addr>` - Data flow analysis
- `ssa <file> <addr>` - Convert to SSA form
- `types <file> <addr>` - Infer VB6 types
- `decompile <file> <addr>` - Decompile to VB6 code
- `project-create <proj> <bin>` - Create project database
- `project-open <proj>` - Open project
- `project-save <proj> <file>` - Save analysis to project

## Common Tasks

### Adding a New CLI Command

1. Add enum variant to `Command` in `core/src/main.zig`
2. Add case to `parseCommand()`
3. Implement `cmdYourCommand()` function
4. Add case to main switch
5. Update `printUsage()`

### Adding New Analysis

1. Create file in `core/src/analysis/`
2. Define analyzer struct with `init()` and `analyze()` methods
3. Store CFG/P-code references
4. Implement using unmanaged ArrayList (see gotchas above!)
5. Add printer in `dataflow_printer.zig` if needed

### Working with P-code

P-code operations are in `core/src/ir/pcode_ops.zig`:
- `PCodeOp` - Operation enum
- `Varnode` - Storage location (register, memory, temp)
- `PCodeInstruction` - Single P-code instruction

Translator: `core/src/ir/x86_translator.zig` converts x86 → P-code

## Testing

Currently manual testing with CLI commands. Future: Add Zig unit tests.

```bash
cd core
zig build
./zig-out/bin/vbdecomp help
./zig-out/bin/vbdecomp analyze <some-vb6-exe>
```

## Git Workflow

- Main branch: `main`
- Commit style: Descriptive with "Phase N:" prefix for major milestones
- Always run `zig build` before committing

```bash
cd core && zig build
git add <files>
git commit -m "Phase N: Description

- Bullet point summary
- Another point"
```

## Completed Phases

- ✅ Phase 1: Foundation (PE parser, VB6 detector, C API)
- ✅ Phase 2: x86 Disassembler
- ✅ Phase 3: Control Flow Analysis
- ✅ Phase 4: P-code IR
- ✅ Phase 5: Data Flow Analysis
- ✅ Phase 6: SSA Conversion
- ✅ Phase 7: Type Inference
- ✅ Phase 8: High-level Code Generation (Decompiler)
- ✅ Phase 9: SQLite Project Database

## Next Steps (TODO)

1. **GUI Development**: Qt-based frontend
2. **VB6 Specifics**: Forms, controls, events, COM
3. **P-code Bytecode**: Support VB6 P-code interpreter
4. **Testing**: Add comprehensive unit tests
5. **Documentation**: User guide and API reference

## Known Issues / Limitations

- Currently targets native code only (P-code bytecode TODO)
- No VB6 runtime library analysis yet
- No GUI yet (CLI only)
- Decompiler output quality needs refinement
- SQLite linking requires `libsqlite3-dev` package

### SQLite Setup

The project requires SQLite3 development libraries:

```bash
# Debian/Ubuntu
sudo apt-get install libsqlite3-dev

# Fedora/RHEL
sudo dnf install sqlite-devel

# macOS
brew install sqlite3

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-sqlite3
```

Build system links with `-lsqlite3` via:
```zig
exe.root_module.linkSystemLibrary("sqlite3", .{});
```

## Debugging Tips

### Build Fails with ArrayList Errors

→ Check for `.init()` calls or missing allocator in `append()`/`deinit()`

### "No field named 'target'" Error

→ Should be `edge.to`, not `edge.target`

### "Expected type expression, found '{'"

→ Usually a missing/extra brace earlier in the file. Check context around the error line.

### Undefined Reference Errors

→ Ensure all new files are added to the build (Zig auto-discovers in `src/`)

## Resources

- Zig 0.16 docs: https://ziglang.org/documentation/0.16.0/
- Ghidra P-code: https://ghidra.re/courses/languages/html/pcoderef.html
- VB6 format: See `docs/vb6-format.md`
- Architecture: See `docs/architecture.md`

## Contact / Feedback

Report issues: https://github.com/anomalyco/opencode

---

**Last Updated**: Phase 9 completion (SQLite Project Database)  
**Zig Version**: 0.16.0  
**Status**: Active development
