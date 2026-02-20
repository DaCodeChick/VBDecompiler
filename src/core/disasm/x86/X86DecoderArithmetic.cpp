// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "X86Disassembler.h"
#include <cstdint>

namespace VBDecompiler {

// Decode arithmetic instructions (ADD, SUB, CMP)
bool X86Disassembler::decodeArithmetic(std::span<const uint8_t> data, size_t& offset,
                                        X86Instruction& instr, uint8_t opcode) {
    (void)data;    // Unused - simplified implementation
    (void)offset;  // Unused - simplified implementation
    
    // Simplified - map opcode ranges to operation
    if (opcode <= 0x05) {
        instr.setOpcode(X86Opcode::ADD);
    }
    else if (opcode >= 0x28 && opcode <= 0x2D) {
        instr.setOpcode(X86Opcode::SUB);
    }
    else if (opcode >= 0x38 && opcode <= 0x3D) {
        instr.setOpcode(X86Opcode::CMP);
    }
    
    // Full operand decoding would go here
    return true;
}

// Decode INC/DEC instructions
bool X86Disassembler::decodeIncDec(std::span<const uint8_t> data, size_t& offset,
                                    X86Instruction& instr, uint8_t opcode) {
    
    if (opcode >= 0x40 && opcode <= 0x47) {
        // INC r32 (0x40-0x47)
        instr.setOpcode(X86Opcode::INC);
        instr.addOperand(makeRegisterOperand(getReg32(opcode - 0x40), 4));
    }
    else if (opcode >= 0x48 && opcode <= 0x4F) {
        // DEC r32 (0x48-0x4F)
        instr.setOpcode(X86Opcode::DEC);
        instr.addOperand(makeRegisterOperand(getReg32(opcode - 0x48), 4));
    }
    else if (opcode == 0xFE) {
        // INC/DEC r/m8
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        // reg field determines operation: 0=INC, 1=DEC
        if (reg == 0) {
            instr.setOpcode(X86Opcode::INC);
        } else if (reg == 1) {
            instr.setOpcode(X86Opcode::DEC);
        } else {
            return false; // Invalid
        }
        
        X86Operand operand;
        if (mod == 3) {
            operand = makeRegisterOperand(getReg8(rm), 1);
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 1, operand)) return false;
        }
        instr.addOperand(operand);
    }
    else if (opcode == 0xFF) {
        // INC/DEC r/m32
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        // reg field determines operation: 0=INC, 1=DEC
        if (reg == 0) {
            instr.setOpcode(X86Opcode::INC);
        } else if (reg == 1) {
            instr.setOpcode(X86Opcode::DEC);
        } else {
            return false; // Invalid (other encodings like CALL/JMP use FF with different reg values)
        }
        
        X86Operand operand;
        if (mod == 3) {
            operand = makeRegisterOperand(getReg32(rm), 4);
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, operand)) return false;
        }
        instr.addOperand(operand);
    }
    
    return true;
}

} // namespace VBDecompiler
