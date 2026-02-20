// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT
//
// RDSS Refactoring: Extract common operand creation and ModR/M decoding patterns

#include "X86Disassembler.h"

namespace VBDecompiler {

// ============================================================================
// Operand Creation Helpers (RDSS: Eliminate 4-line pattern duplication)
// ============================================================================

X86Operand X86Disassembler::makeRegisterOperand(X86Register reg, uint8_t size) {
    X86Operand op;
    op.type = X86OperandType::REGISTER;
    op.reg = reg;
    op.size = size;
    return op;
}

X86Operand X86Disassembler::makeImmediateOperand(uint32_t value, uint8_t size) {
    X86Operand op;
    op.type = X86OperandType::IMMEDIATE;
    op.immediate = value;
    op.size = size;
    return op;
}

// ============================================================================
// ModR/M Operand Decoding Helpers (RDSS: Simplify 15-20 line pattern)
// ============================================================================

/**
 * @brief Decode ModR/M operands: r/m (dst), reg (src)
 * 
 * This helper eliminates the common pattern:
 *   - Decode ModR/M byte
 *   - Check if mod == 3 (register) or memory
 *   - Create appropriate register/memory operand for dst
 *   - Create register operand for src
 * 
 * Used by: MOV r/m, r; XOR r/m, r; AND r/m, r; OR r/m, r; TEST r/m, r; etc.
 */
bool X86Disassembler::decodeModRMtoRegOperands(
    std::span<const uint8_t> data,
    size_t& offset,
    uint8_t size,
    X86Operand& dst,
    X86Operand& src) {
    
    uint8_t mod, reg, rm;
    if (!decodeModRM(data, offset, mod, reg, rm)) {
        return false;
    }
    
    // Decode destination (r/m - register or memory)
    if (mod == 3) {
        // Register mode
        X86Register dstReg = (size == 1) ? getReg8(rm) :
                             (size == 2) ? getReg16(rm) : getReg32(rm);
        dst = makeRegisterOperand(dstReg, size);
    } else {
        // Memory mode
        if (!decodeMemoryOperand(data, offset, mod, rm, size, dst)) {
            return false;
        }
    }
    
    // Decode source (reg - always register)
    X86Register srcReg = (size == 1) ? getReg8(reg) :
                         (size == 2) ? getReg16(reg) : getReg32(reg);
    src = makeRegisterOperand(srcReg, size);
    
    return true;
}

/**
 * @brief Decode ModR/M operands: reg (dst), r/m (src)
 * 
 * This helper eliminates the common pattern for reverse direction.
 * 
 * Used by: MOV r, r/m; XOR r, r/m; AND r, r/m; OR r, r/m; etc.
 */
bool X86Disassembler::decodeRegtoModRMOperands(
    std::span<const uint8_t> data,
    size_t& offset,
    uint8_t size,
    X86Operand& dst,
    X86Operand& src) {
    
    uint8_t mod, reg, rm;
    if (!decodeModRM(data, offset, mod, reg, rm)) {
        return false;
    }
    
    // Decode destination (reg - always register)
    X86Register dstReg = (size == 1) ? getReg8(reg) :
                         (size == 2) ? getReg16(reg) : getReg32(reg);
    dst = makeRegisterOperand(dstReg, size);
    
    // Decode source (r/m - register or memory)
    if (mod == 3) {
        // Register mode
        X86Register srcReg = (size == 1) ? getReg8(rm) :
                             (size == 2) ? getReg16(rm) : getReg32(rm);
        src = makeRegisterOperand(srcReg, size);
    } else {
        // Memory mode
        if (!decodeMemoryOperand(data, offset, mod, rm, size, src)) {
            return false;
        }
    }
    
    return true;
}

} // namespace VBDecompiler
