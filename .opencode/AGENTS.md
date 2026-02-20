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

### C++ Standards
- **Language**: C++23 only
- **Standard types**: Use `std::` types (`uint8_t`, `std::span`, etc.)
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

## Examples of Good Refactoring

### Before (Spaghetti):
```cpp
bool decode(uint8_t* data, size_t len, Instruction& out) {
    if (len < 1) return false;
    uint8_t op = data[0];
    if (op == 0x68) {
        if (len < 5) return false;
        out.type = PUSH;
        out.imm = *(uint32_t*)(data+1);
        return true;
    } else if (op == 0xE8) {
        if (len < 5) return false;
        out.type = CALL;
        out.offset = *(int32_t*)(data+1);
        return true;
    } else if (op >= 0x50 && op <= 0x57) {
        out.type = PUSH;
        out.reg = op - 0x50;
        return true;
    }
    // ... 50 more else-if blocks
}
```

### After (Clean):
```cpp
bool decode(std::span<const uint8_t> data, Instruction& out) {
    if (data.empty()) return false;
    
    uint8_t opcode = data[0];
    size_t offset = 1;
    
    // Dispatch to specialized decoders
    if (opcode == 0x68 || opcode == 0x6A) {
        return decodePushImmediate(data, offset, out, opcode);
    }
    if (opcode == 0xE8) {
        return decodeCallNear(data, offset, out);
    }
    if (opcode >= 0x50 && opcode <= 0x57) {
        return decodePushRegister(data, offset, out, opcode);
    }
    
    // ... dispatch to other decoders
    return false;
}

bool decodePushImmediate(std::span<const uint8_t> data, size_t& offset,
                         Instruction& out, uint8_t opcode) {
    out.setOpcode(Opcode::PUSH);
    
    if (opcode == 0x68) {
        // PUSH imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        out.addImmediateOperand(imm, 4);
    } else {
        // PUSH imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        out.addImmediateOperand(static_cast<int32_t>(static_cast<int8_t>(imm)), 1);
    }
    
    return true;
}
```

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
