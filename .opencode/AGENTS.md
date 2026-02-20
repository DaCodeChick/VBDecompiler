# Agent Policies for VBDecompiler

This document defines policies and guidelines that AI agents should follow when working on this codebase.

---

## RDSS Policy: Refactor, Despaghettify, Simplify, Split

When working on code in this project, always apply the **RDSS principles**:

### 1. **Refactor** - Improve Code Structure
- Extract repeated code into reusable functions
- Apply DRY (Don't Repeat Yourself) principle
- Use meaningful variable and function names
- Follow consistent naming conventions across the codebase
- Prefer composition over inheritance where appropriate
- **Use C++23 features first** - If C++23 has something we can use, use it. Do not fall back to earlier standards
- **NEVER use raw pointers or arrays** - Make use of the STL for memory safety (`std::unique_ptr`, `std::shared_ptr`, `std::vector`, `std::array`, `std::span`)
- **Always prefer C++ STL over plain C standard library** - This is 2026, not 1998. The code should reflect the modern day
- **Use modern C++23 idioms**: `std::optional`, `std::expected`, `std::ranges`, `std::views`, structured bindings, `contains()` for sets/maps

### 2. **Despaghettify** - Untangle Complex Logic
- Break down deeply nested conditionals into separate functions
- Use early returns to reduce nesting depth
- Replace complex conditional chains with lookup tables or pattern matching
- Eliminate goto statements and convoluted control flow
- Make data flow clear and explicit

### 3. **Simplify** - Reduce Complexity
- Keep functions focused on a single responsibility (SRP)
- Limit function length to ~50 lines where possible
- Reduce cyclomatic complexity (aim for < 10 per function)
- Remove unnecessary abstractions and over-engineering
- Use standard library features instead of custom implementations
- Prefer clear, explicit code over clever tricks

### 4. **Split** - Modularize Large Files
When a source file exceeds **500 lines**, consider splitting it:

#### File Size Thresholds:
- **500-800 lines**: Consider splitting if logical boundaries exist
- **800-1200 lines**: Should be split into multiple files
- **1200+ lines**: Must be split immediately

#### How to Split Files:

**Option A: Vertical Split (by functionality)**
```
X86Disassembler.cpp (1308 lines) →
  - X86Disassembler.cpp          (core logic, ~200 lines)
  - X86DecoderArithmetic.cpp     (ADD, SUB, CMP, etc.)
  - X86DecoderLogical.cpp        (AND, OR, XOR, TEST, etc.)
  - X86DecoderControl.cpp        (JMP, CALL, RET, Jcc, etc.)
  - X86DecoderData.cpp           (MOV, LEA, PUSH, POP, etc.)
  - X86DecoderModRM.cpp          (ModR/M, SIB decoding helpers)
```

**Option B: Horizontal Split (by layer)**
```
LargeClass.cpp →
  - LargeClass.cpp               (public interface)
  - LargeClassImpl.cpp           (implementation details)
  - LargeClassHelpers.cpp        (helper functions)
```

**Option C: Extract Specialized Classes**
```
Monolithic.cpp →
  - MainClass.cpp
  - HelperClass1.cpp
  - HelperClass2.cpp
  - Utilities.cpp
```

#### When to Split:
- ✅ Natural functional boundaries exist (arithmetic vs logical ops)
- ✅ Code can be grouped by related operations
- ✅ Different parts have different testing needs
- ✅ Multiple engineers might work on different sections
- ❌ Don't split if it creates artificial dependencies
- ❌ Don't split if it obscures the overall logic flow
- ❌ Don't split just to hit arbitrary line counts

---

## Code Organization Guidelines

### File Structure
```
src/
  core/               # Core engine (Qt-independent)
    pe/              # PE file parsing
    vb/              # VB structure parsing  
    disasm/          # Disassemblers
      x86/           # x86 disassembler
      pcode/         # P-Code disassembler (future)
  ui/                # Qt UI components
tests/               # Test programs
docs/                # Documentation
```

### Header vs Implementation
- **Headers (.h)**: Interface declarations, documentation, inline templates
- **Implementation (.cpp)**: Function definitions, heavy logic
- Keep headers minimal - include only what's necessary
- Use forward declarations to reduce header dependencies

### Class Size Guidelines
- **Small class**: < 200 lines total (header + implementation)
- **Medium class**: 200-500 lines
- **Large class**: 500-1000 lines (consider splitting)
- **Huge class**: 1000+ lines (must split or justify)

---

## Current Files Needing Attention

### Immediate Candidates for Refactoring:

#### `src/core/disasm/x86/X86Disassembler.cpp` - **1308 lines** 🔴
**Status**: MUST SPLIT

**Recommended split**:
```cpp
// Core disassembler + dispatch
X86Disassembler.cpp              (~250 lines)
  - disassembleOne()
  - disassemble()
  - disassembleFunction()
  - Main opcode dispatch logic

// Decoder groups (by instruction category)
X86DecoderData.cpp               (~250 lines)
  - decodeMov()
  - decodeLea()
  - decodePush()
  - decodePop()

X86DecoderArithmetic.cpp         (~200 lines)
  - decodeAdd()
  - decodeSub()
  - decodeCmp()
  - decodeIncDec()

X86DecoderLogical.cpp            (~250 lines)
  - decodeAnd()
  - decodeOr()
  - decodeXor()
  - decodeTest()

X86DecoderControl.cpp            (~200 lines)
  - decodeCall()
  - decodeJmp()
  - decodeJcc()
  - decodeRet()

X86DecoderHelpers.cpp            (~150 lines)
  - decodeModRM()
  - decodeSIB()
  - decodeMemoryOperand()
  - Register lookup functions
```

**Benefits**:
- Easier to navigate and maintain
- Clear separation of concerns
- Easier to test individual decoder groups
- Multiple developers can work in parallel
- Faster compilation times (smaller translation units)

---

## Implementation Standards

### C++ Standards (2026)
- **Language**: C++23 only - use the latest standard features
- **Year**: This is 2026, not 2024 - update copyright headers and documentation accordingly
- **Standard types**: Use `std::` types (`uint8_t`, `std::span`, `std::optional`, etc.)
- **Memory safety**: NEVER use raw pointers or C-style arrays
  - Use `std::unique_ptr` / `std::shared_ptr` for ownership
  - Use `std::vector` / `std::array` for collections
  - Use `std::span` for non-owning views
  - Use `std::reference_wrapper` instead of raw pointer references
- **Modern idioms**: 
  - Use `.contains()` instead of `.find() != .end()` for set/map membership tests (C++20)
  - Use `std::ranges` and `std::views` for algorithms (C++20/23)
  - Use structured bindings for multi-value returns
  - Use `std::expected` for error handling (C++23)
  - Use `std::optional` for nullable values
- **C++ STL over C stdlib**: Always prefer C++ alternatives
  - Use `std::string` not `char*`
  - Use `std::vector` not `malloc/new[]`
  - Use `<cmath>` not `<math.h>`
  - Use `std::unique_ptr` not `new/delete`
- **Qt types**: Only for Qt-specific APIs (UI, QFile, QDataStream)
- **Core engine**: Keep Qt-independent where possible

### Naming Conventions
- **Classes**: PascalCase (`X86Disassembler`)
- **Functions**: camelCase (`decodeModRM`)
- **Variables**: camelCase (`opcode`, `byteValue`)
- **Constants**: UPPER_SNAKE_CASE (`MAX_CODE_SIZE`)
- **Namespaces**: PascalCase (`VBDecompiler`)

### Documentation
- Document complex algorithms in comments
- Add function comments for public APIs
- Keep inline comments concise and relevant
- Update documentation when refactoring

### Testing
- Write test cases for new functionality
- Run existing tests after refactoring
- Add regression tests for bug fixes

---

## Refactoring Checklist

Before committing refactored code, verify:

- [ ] Code compiles without errors or new warnings
- [ ] Existing tests pass
- [ ] New code is properly tested
- [ ] Functions are focused and single-purpose
- [ ] Complex logic is broken into smaller functions
- [ ] File size is reasonable (< 800 lines preferred)
- [ ] No code duplication
- [ ] Naming is clear and consistent
- [ ] Comments explain "why", not "what"
- [ ] Dependencies are minimal

---

## Memory Safety & Modern C++ (2026)

### Never Use These (Banned Patterns):
```cpp
❌ int* ptr = new int;                    // Use std::unique_ptr<int>
❌ int arr[100];                          // Use std::array<int, 100>
❌ void func(int* data, size_t len);     // Use std::span<int>
❌ char* str = malloc(100);               // Use std::string or std::vector<char>
❌ if (map.find(x) != map.end())         // Use map.contains(x)
❌ delete ptr;                            // Let smart pointers handle it
❌ memset(arr, 0, sizeof(arr));          // Use std::fill or value-initialization
```

### Always Use These (Modern Alternatives):
```cpp
✅ auto ptr = std::make_unique<int>();                    // Unique ownership
✅ auto arr = std::array<int, 100>{};                     // Fixed-size array
✅ void func(std::span<int> data);                        // Non-owning view
✅ auto str = std::string{};                              // Dynamic string
✅ if (map.contains(x))                                   // C++20 contains()
✅ // Smart pointers auto-cleanup                         // RAII
✅ std::ranges::fill(arr, 0);                             // C++20 ranges
✅ auto vec = std::vector<int>(100);                      // Dynamic array
✅ auto result = std::expected<T, Error>{};               // C++23 error handling
✅ auto opt = std::optional<int>{};                       // Nullable value
✅ auto [key, value] = *map.find(x);                      // Structured bindings
```

### Smart Pointer Guidelines:
- **`std::unique_ptr<T>`**: Exclusive ownership, no sharing
  - Use by default for owned heap objects
  - `auto obj = std::make_unique<MyClass>(args...);`
- **`std::shared_ptr<T>`**: Shared ownership with reference counting
  - Use sparingly - only when true sharing is needed
  - `auto obj = std::make_shared<MyClass>(args...);`
- **`std::weak_ptr<T>`**: Non-owning observer of `shared_ptr`
  - Use to break circular references
- **Raw pointers**: Only for non-owning observation (prefer `std::reference_wrapper` or `T*`)
  - NEVER store raw pointers in containers for comparison/tracking
  - Use IDs or indices instead

### Container Best Practices:
```cpp
// ✅ Good: ID-based tracking (stable, safe)
std::unordered_set<uint32_t> visitedIds;
visitedIds.insert(block->getId());
if (visitedIds.contains(blockId)) { ... }

// ❌ Bad: Pointer-based tracking (unstable, dangerous)
std::unordered_set<const Block*> visited;  // Pointers can be invalidated!
visited.insert(block);
if (visited.find(block) != visited.end()) { ... }  // Old-style too!

// ✅ Good: Modern iteration with ranges
for (const auto& [key, value] : map) {  // Structured binding
    processItem(key, value);
}

// ✅ Good: Filtering with views (C++20)
auto evenNumbers = numbers 
    | std::views::filter([](int n) { return n % 2 == 0; })
    | std::views::transform([](int n) { return n * 2; });

// ✅ Good: Safe array access
std::span<const uint8_t> safeView(data, size);
if (safeView.size() >= 4) {
    uint32_t value = safeView[0];  // Bounds-checkable
}
```

---

## Examples of Good Refactoring

### Before (Spaghetti - Old C++98 Style):
```cpp
bool decode(uint8_t* data, size_t len, Instruction& out) {
    if (len < 1) return false;
    uint8_t op = data[0];
    if (op == 0x68) {
        if (len < 5) return false;
        out.type = PUSH;
        out.imm = *(uint32_t*)(data+1);  // UNSAFE! Raw pointer arithmetic
        return true;
    } else if (op == 0xE8) {
        if (len < 5) return false;
        out.type = CALL;
        out.offset = *(int32_t*)(data+1);  // UNSAFE!
        return true;
    } else if (op >= 0x50 && op <= 0x57) {
        out.type = PUSH;
        out.reg = op - 0x50;
        return true;
    }
    // ... 50 more else-if blocks
}
```

### After (Clean - Modern C++23):
```cpp
std::optional<Instruction> decode(std::span<const uint8_t> data) {
    if (data.empty()) {
        return std::nullopt;
    }
    
    uint8_t opcode = data[0];
    auto remaining = data.subspan(1);
    
    // Dispatch to specialized decoders (early returns reduce nesting)
    if (opcode == 0x68 || opcode == 0x6A) {
        return decodePushImmediate(remaining, opcode);
    }
    if (opcode == 0xE8) {
        return decodeCallNear(remaining);
    }
    if (opcode >= 0x50 && opcode <= 0x57) {
        return decodePushRegister(opcode);
    }
    
    // ... dispatch to other decoders
    return std::nullopt;
}

std::optional<Instruction> decodePushImmediate(std::span<const uint8_t> data,
                                                uint8_t opcode) {
    Instruction out;
    out.setOpcode(Opcode::PUSH);
    
    if (opcode == 0x68) {
        // PUSH imm32
        if (auto imm = readDword(data)) {  // C++17 if-with-initializer
            out.addImmediateOperand(*imm, 4);
            return out;
        }
    } else {
        // PUSH imm8
        if (auto imm = readByte(data)) {
            out.addImmediateOperand(static_cast<int32_t>(static_cast<int8_t>(*imm)), 1);
            return out;
        }
    }
    
    return std::nullopt;
}

// Helper using std::optional instead of bool+out-param
std::optional<uint32_t> readDword(std::span<const uint8_t> data) {
    if (data.size() < 4) {
        return std::nullopt;
    }
    
    // Safe: no pointer arithmetic, bounds-checked via span
    uint32_t value = 0;
    std::memcpy(&value, data.data(), sizeof(uint32_t));
    return value;
}
```

**Key improvements in modern version:**
- ✅ No raw pointers - uses `std::span` for safe array views
- ✅ Uses `std::optional` for nullable returns (C++17)
- ✅ Bounds checking via span - no buffer overflows
- ✅ Early returns reduce nesting depth
- ✅ Clear separation of concerns
- ✅ Type-safe with no pointer arithmetic
- ✅ Modern error handling (optional vs bool+out-param)

---

## When NOT to Refactor

Sometimes it's better to leave code as-is:

- ❌ Performance-critical hot paths (profile first!)
- ❌ Code that works and is well-tested (if it ain't broke...)
- ❌ Near release deadlines (defer to next cycle)
- ❌ Third-party code (maintain separately)
- ❌ Legacy code with no tests (add tests first)

---

## Summary

**Remember RDSS**:
1. **Refactor** existing code to improve quality
2. **Despaghettify** complex control flow
3. **Simplify** overly complex logic
4. **Split** large files (500+ lines) into focused modules

When in doubt, ask: "Will this make the code easier to understand and maintain?"
