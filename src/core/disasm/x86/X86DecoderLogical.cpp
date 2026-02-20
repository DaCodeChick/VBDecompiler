// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "X86Disassembler.h"
#include <cstdint>

namespace VBDecompiler {

// Decode TEST instruction (Logical compare)
bool X86Disassembler::decodeTest(std::span<const uint8_t> data, size_t& offset,
                                  X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::TEST);
    
    if (opcode == 0x84) {
        // TEST r/m8, r8
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            // Register to register
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg8(rm);
            dst.size = 1;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 1, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg8(reg);
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x85) {
        // TEST r/m32, r32
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            // Register to register
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg32(rm);
            dst.size = 4;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg32(reg);
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0xA8) {
        // TEST AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::AL;
        dst.size = 1;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0xA9) {
        // TEST EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::EAX;
        dst.size = 4;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    
    return true;
}

// Decode XOR instruction (Exclusive OR)
bool X86Disassembler::decodeXor(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::XOR);
    
    if (opcode == 0x30) {
        // XOR r/m8, r8
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg8(rm);
            dst.size = 1;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 1, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg8(reg);
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x31) {
        // XOR r/m32, r32
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg32(rm);
            dst.size = 4;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg32(reg);
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x32) {
        // XOR r8, r/m8
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
    }
    else if (opcode == 0x33) {
        // XOR r32, r/m32
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
    }
    else if (opcode == 0x34) {
        // XOR AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::AL;
        dst.size = 1;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x35) {
        // XOR EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::EAX;
        dst.size = 4;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    
    return true;
}

// Decode AND instruction (Logical AND)
bool X86Disassembler::decodeAnd(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::AND);
    
    if (opcode == 0x20) {
        // AND r/m8, r8
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg8(rm);
            dst.size = 1;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 1, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg8(reg);
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x21) {
        // AND r/m32, r32
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg32(rm);
            dst.size = 4;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg32(reg);
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x22) {
        // AND r8, r/m8
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
    }
    else if (opcode == 0x23) {
        // AND r32, r/m32
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
    }
    else if (opcode == 0x24) {
        // AND AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::AL;
        dst.size = 1;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x25) {
        // AND EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::EAX;
        dst.size = 4;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    
    return true;
}

// Decode OR instruction (Logical OR)
bool X86Disassembler::decodeOr(std::span<const uint8_t> data, size_t& offset,
                                X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::OR);
    
    if (opcode == 0x08) {
        // OR r/m8, r8
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg8(rm);
            dst.size = 1;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 1, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg8(reg);
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x09) {
        // OR r/m32, r32
        uint8_t mod, reg, rm;
        if (!decodeModRM(data, offset, mod, reg, rm)) return false;
        
        X86Operand dst, src;
        if (mod == 3) {
            dst.type = X86OperandType::REGISTER;
            dst.reg = getReg32(rm);
            dst.size = 4;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, dst)) return false;
        }
        
        src.type = X86OperandType::REGISTER;
        src.reg = getReg32(reg);
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x0A) {
        // OR r8, r/m8
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
    }
    else if (opcode == 0x0B) {
        // OR r32, r/m32
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
    }
    else if (opcode == 0x0C) {
        // OR AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::AL;
        dst.size = 1;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 1;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x0D) {
        // OR EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        
        X86Operand dst;
        dst.type = X86OperandType::REGISTER;
        dst.reg = X86Register::EAX;
        dst.size = 4;
        
        X86Operand src;
        src.type = X86OperandType::IMMEDIATE;
        src.immediate = imm;
        src.size = 4;
        
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    
    return true;
}

} // namespace VBDecompiler
