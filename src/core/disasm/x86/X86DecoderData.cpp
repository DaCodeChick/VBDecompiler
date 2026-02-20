// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "X86Disassembler.h"
#include <cstdint>

namespace VBDecompiler {

// Decode MOV instruction (Data Transfer)
bool X86Disassembler::decodeMov(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::MOV);
    
    X86Operand dst, src;
    
    // MOV r/m8, r8 (0x88)
    if (opcode == 0x88) {
        // Note: MOV has reversed operand order (r/m, reg) vs typical (reg, r/m)
        if (!decodeRegtoModRMOperands(data, offset, 1, src, dst)) return false;
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    // MOV r/m32, r32 (0x89)
    else if (opcode == 0x89) {
        if (!decodeRegtoModRMOperands(data, offset, 4, src, dst)) return false;
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    // MOV r8, r/m8 (0x8A)
    else if (opcode == 0x8A) {
        if (!decodeRegtoModRMOperands(data, offset, 1, dst, src)) return false;
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    // MOV r32, r/m32 (0x8B)
    else if (opcode == 0x8B) {
        if (!decodeRegtoModRMOperands(data, offset, 4, dst, src)) return false;
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    // MOV reg, imm (0xB0-0xBF)
    else if (opcode >= 0xB0 && opcode <= 0xBF) {
        if (opcode <= 0xB7) {
            // MOV r8, imm8
            uint8_t imm;
            if (!readByte(data, offset, imm)) return false;
            dst = makeRegisterOperand(getReg8(opcode & 0x07), 1);
            src = makeImmediateOperand(imm, 1);
        } else {
            // MOV r32, imm32
            uint32_t imm;
            if (!readDword(data, offset, imm)) return false;
            dst = makeRegisterOperand(getReg32(opcode & 0x07), 4);
            src = makeImmediateOperand(imm, 4);
        }
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    // MOV r/m, imm (0xC6-0xC7)
    else if (opcode == 0xC6 || opcode == 0xC7) {
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        // reg field must be 0 for MOV
        if (reg != 0) {
            setError("Invalid MOV r/m, imm encoding");
            return false;
        }
        
        if (opcode == 0xC6) {
            // MOV r/m8, imm8
            uint8_t size = 1;
            if (mod == 3) {
                dst = makeRegisterOperand(getReg8(rm), size);
            } else {
                if (!decodeMemoryOperand(data, offset, mod, rm, size, dst)) return false;
            }
            uint8_t imm;
            if (!readByte(data, offset, imm)) return false;
            src = makeImmediateOperand(imm, size);
        } else {
            // MOV r/m32, imm32
            uint8_t size = 4;
            if (mod == 3) {
                dst = makeRegisterOperand(getReg32(rm), size);
            } else {
                if (!decodeMemoryOperand(data, offset, mod, rm, size, dst)) return false;
            }
            uint32_t imm;
            if (!readDword(data, offset, imm)) return false;
            src = makeImmediateOperand(imm, size);
        }
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    
    return true;
}

// Decode LEA instruction (Load Effective Address)
bool X86Disassembler::decodeLea(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    (void)opcode;  // Unused
    instr.setOpcode(X86Opcode::LEA);
    
    // LEA r32, m - opcode 0x8D
    // Only memory operands are valid for LEA (not registers)
    uint8_t mod, reg, rm;
    if (!decodeModRM(data, offset, mod, reg, rm)) return false;
    
    // Destination register
    instr.addOperand(makeRegisterOperand(getReg32(reg), 4));
    
    // Source must be memory operand
    X86Operand src;
    if (!decodeMemoryOperand(data, offset, mod, rm, 4, src)) return false;
    instr.addOperand(src);
    
    return true;
}

// Decode PUSH instruction
bool X86Disassembler::decodePush(std::span<const uint8_t> data, size_t& offset,
                                  X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::PUSH);
    
    // PUSH r32 (0x50+r)
    if (opcode >= 0x50 && opcode <= 0x57) {
        uint8_t regNum = opcode & 0x07;
        instr.addOperand(makeRegisterOperand(getReg32(regNum), 4));
    }
    // PUSH imm32 (0x68)
    else if (opcode == 0x68) {
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        instr.addOperand(makeImmediateOperand(imm, 4));
    }
    // PUSH imm8 (0x6A) - sign-extended to 32-bit
    else if (opcode == 0x6A) {
        int8_t imm;
        if (!readSByte(data, offset, imm)) return false;
        instr.addOperand(makeImmediateOperand(static_cast<uint32_t>(static_cast<int32_t>(imm)), 1));
    }
    
    return true;
}

// Decode POP instruction
bool X86Disassembler::decodePop(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    (void)data;    // Unused
    (void)offset;  // Unused
    
    instr.setOpcode(X86Opcode::POP);
    
    // POP r32 (0x58+r)
    uint8_t regNum = opcode & 0x07;
    instr.addOperand(makeRegisterOperand(getReg32(regNum), 4));
    
    return true;
}

} // namespace VBDecompiler
