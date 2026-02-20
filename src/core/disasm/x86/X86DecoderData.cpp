// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "X86Disassembler.h"
#include <cstdint>

namespace VBDecompiler {

// Decode MOV instruction (Data Transfer)
bool X86Disassembler::decodeMov(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::MOV);
    
    // MOV r/m8, r8 (0x88)
    if (opcode == 0x88) {
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        src.type = X86OperandType::REGISTER;
        src.reg = getReg8(reg);
        src.size = 1;
        
        if (mod == 3) {
            // Register to register
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg8(rm);
            dst.size = 1;
        } else {
            // Register to memory
            if (!decodeMemoryOperand(data, offset, mod, rm, 1, dst)) return false;
        }
        
        instr.addOperand(dst);
        instr.addOperand(src);
        return true;
    }
    // MOV r/m32, r32 (0x89)
    else if (opcode == 0x89) {
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        src.type = X86OperandType::REGISTER;
        src.reg = getReg32(reg);
        src.size = 4;
        
        if (mod == 3) {
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg32(rm);
            dst.size = 4;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, dst)) return false;
        }
        
        instr.addOperand(dst);
        instr.addOperand(src);
        return true;
    }
    // MOV r8, r/m8 (0x8A)
    else if (opcode == 0x8A) {
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        dst.type = X86OperandType::REGISTER;
        dst.reg = getReg8(reg);
        dst.size = 1;
        
        if (mod == 3) {
            src.type = X86OperandType::REGISTER;
            src.reg = getReg8(rm);
            src.size = 1;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 1, src)) return false;
        }
        
        instr.addOperand(dst);
        instr.addOperand(src);
        return true;
    }
    // MOV r32, r/m32 (0x8B)
    else if (opcode == 0x8B) {
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        dst.type = X86OperandType::REGISTER;
        dst.reg = getReg32(reg);
        dst.size = 4;
        
        if (mod == 3) {
            src.type = X86OperandType::REGISTER;
            src.reg = getReg32(rm);
            src.size = 4;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, src)) return false;
        }
        
        instr.addOperand(dst);
        instr.addOperand(src);
        return true;
    }
    // MOV reg, imm (0xB0-0xBF)
    else if (opcode >= 0xB0 && opcode <= 0xBF) {
        X86Operand dst, src;
        dst.type = X86OperandType::REGISTER;
        src.type = X86OperandType::IMMEDIATE;
        
        if (opcode <= 0xB7) {
            // MOV r8, imm8
            dst.reg = getReg8(opcode & 0x07);
            dst.size = 1;
            uint8_t imm;
            if (!readByte(data, offset, imm)) return false;
            src.immediate = imm;
            src.size = 1;
        } else {
            // MOV r32, imm32
            dst.reg = getReg32(opcode & 0x07);
            dst.size = 4;
            uint32_t imm;
            if (!readDword(data, offset, imm)) return false;
            src.immediate = imm;
            src.size = 4;
        }
        
        instr.addOperand(dst);
        instr.addOperand(src);
        return true;
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
        
        X86Operand dst, src;
        src.type = X86OperandType::IMMEDIATE;
        
        if (opcode == 0xC6) {
            // MOV r/m8, imm8
            if (mod == 3) {
                dst.type = X86OperandType::REGISTER;
                dst.reg = getReg8(rm);
                dst.size = 1;
            } else {
                if (!decodeMemoryOperand(data, offset, mod, rm, 1, dst)) return false;
            }
            uint8_t imm;
            if (!readByte(data, offset, imm)) return false;
            src.immediate = imm;
            src.size = 1;
        } else {
            // MOV r/m32, imm32
            if (mod == 3) {
                dst.type = X86OperandType::REGISTER;
                dst.reg = getReg32(rm);
                dst.size = 4;
            } else {
                if (!decodeMemoryOperand(data, offset, mod, rm, 4, dst)) return false;
            }
            uint32_t imm;
            if (!readDword(data, offset, imm)) return false;
            src.immediate = imm;
            src.size = 4;
        }
        
        instr.addOperand(dst);
        instr.addOperand(src);
        return true;
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
    X86Operand dst;
    dst.type = X86OperandType::REGISTER;
    dst.reg = getReg32(reg);
    dst.size = 4;
    instr.addOperand(dst);
    
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
    
    X86Operand operand;
    
    // PUSH r32 (0x50+r)
    if (opcode >= 0x50 && opcode <= 0x57) {
        uint8_t regNum = opcode & 0x07;
        operand.type = X86OperandType::REGISTER;
        operand.reg = getReg32(regNum);
        operand.size = 4;
    }
    // PUSH imm32 (0x68)
    else if (opcode == 0x68) {
        operand.type = X86OperandType::IMMEDIATE;
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        operand.immediate = imm;
        operand.size = 4;
    }
    // PUSH imm8 (0x6A) - sign-extended to 32-bit
    else if (opcode == 0x6A) {
        operand.type = X86OperandType::IMMEDIATE;
        int8_t imm;
        if (!readSByte(data, offset, imm)) return false;
        operand.immediate = static_cast<uint32_t>(static_cast<int32_t>(imm));  // Sign extend
        operand.size = 1;
    }
    
    instr.addOperand(operand);
    return true;
}

// Decode POP instruction
bool X86Disassembler::decodePop(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::POP);
    
    // POP r32 (0x58+r)
    uint8_t regNum = opcode & 0x07;
    X86Operand operand;
    operand.type = X86OperandType::REGISTER;
    operand.reg = getReg32(regNum);
    operand.size = 4;
    instr.addOperand(operand);
    
    return true;
}

} // namespace VBDecompiler
