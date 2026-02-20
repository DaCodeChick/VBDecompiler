// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "X86Disassembler.h"
#include <cstdint>

namespace VBDecompiler {

// Decode TEST instruction (Logical compare)
bool X86Disassembler::decodeTest(std::span<const uint8_t> data, size_t& offset,
                                  X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::TEST);
    
    X86Operand dst, src;
    
    if (opcode == 0x84) {
        // TEST r/m8, r8
        if (!decodeModRMtoRegOperands(data, offset, 1, dst, src)) return false;
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0x85) {
        // TEST r/m32, r32
        if (!decodeModRMtoRegOperands(data, offset, 4, dst, src)) return false;
        instr.addOperand(dst);
        instr.addOperand(src);
    }
    else if (opcode == 0xA8) {
        // TEST AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        instr.addOperand(makeRegisterOperand(X86Register::AL, 1));
        instr.addOperand(makeImmediateOperand(imm, 1));
    }
    else if (opcode == 0xA9) {
        // TEST EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        instr.addOperand(makeRegisterOperand(X86Register::EAX, 4));
        instr.addOperand(makeImmediateOperand(imm, 4));
    }
    
    return true;
}

// Decode XOR instruction (Exclusive OR)
bool X86Disassembler::decodeXor(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::XOR);
    
    X86Operand dst, src;
    
    if (opcode == 0x30) {
        // XOR r/m8, r8
        if (!decodeModRMtoRegOperands(data, offset, 1, dst, src)) return false;
    }
    else if (opcode == 0x31) {
        // XOR r/m32, r32
        if (!decodeModRMtoRegOperands(data, offset, 4, dst, src)) return false;
    }
    else if (opcode == 0x32) {
        // XOR r8, r/m8
        if (!decodeRegtoModRMOperands(data, offset, 1, dst, src)) return false;
    }
    else if (opcode == 0x33) {
        // XOR r32, r/m32
        if (!decodeRegtoModRMOperands(data, offset, 4, dst, src)) return false;
    }
    else if (opcode == 0x34) {
        // XOR AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        dst = makeRegisterOperand(X86Register::AL, 1);
        src = makeImmediateOperand(imm, 1);
    }
    else if (opcode == 0x35) {
        // XOR EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        dst = makeRegisterOperand(X86Register::EAX, 4);
        src = makeImmediateOperand(imm, 4);
    }
    
    instr.addOperand(dst);
    instr.addOperand(src);
    return true;
}

// Decode AND instruction (Logical AND)
bool X86Disassembler::decodeAnd(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::AND);
    
    X86Operand dst, src;
    
    if (opcode == 0x20) {
        // AND r/m8, r8
        if (!decodeModRMtoRegOperands(data, offset, 1, dst, src)) return false;
    }
    else if (opcode == 0x21) {
        // AND r/m32, r32
        if (!decodeModRMtoRegOperands(data, offset, 4, dst, src)) return false;
    }
    else if (opcode == 0x22) {
        // AND r8, r/m8
        if (!decodeRegtoModRMOperands(data, offset, 1, dst, src)) return false;
    }
    else if (opcode == 0x23) {
        // AND r32, r/m32
        if (!decodeRegtoModRMOperands(data, offset, 4, dst, src)) return false;
    }
    else if (opcode == 0x24) {
        // AND AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        dst = makeRegisterOperand(X86Register::AL, 1);
        src = makeImmediateOperand(imm, 1);
    }
    else if (opcode == 0x25) {
        // AND EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        dst = makeRegisterOperand(X86Register::EAX, 4);
        src = makeImmediateOperand(imm, 4);
    }
    
    instr.addOperand(dst);
    instr.addOperand(src);
    return true;
}

// Decode OR instruction (Logical OR)
bool X86Disassembler::decodeOr(std::span<const uint8_t> data, size_t& offset,
                                X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::OR);
    
    X86Operand dst, src;
    
    if (opcode == 0x08) {
        // OR r/m8, r8
        if (!decodeModRMtoRegOperands(data, offset, 1, dst, src)) return false;
    }
    else if (opcode == 0x09) {
        // OR r/m32, r32
        if (!decodeModRMtoRegOperands(data, offset, 4, dst, src)) return false;
    }
    else if (opcode == 0x0A) {
        // OR r8, r/m8
        if (!decodeRegtoModRMOperands(data, offset, 1, dst, src)) return false;
    }
    else if (opcode == 0x0B) {
        // OR r32, r/m32
        if (!decodeRegtoModRMOperands(data, offset, 4, dst, src)) return false;
    }
    else if (opcode == 0x0C) {
        // OR AL, imm8
        uint8_t imm;
        if (!readByte(data, offset, imm)) return false;
        dst = makeRegisterOperand(X86Register::AL, 1);
        src = makeImmediateOperand(imm, 1);
    }
    else if (opcode == 0x0D) {
        // OR EAX, imm32
        uint32_t imm;
        if (!readDword(data, offset, imm)) return false;
        dst = makeRegisterOperand(X86Register::EAX, 4);
        src = makeImmediateOperand(imm, 4);
    }
    
    instr.addOperand(dst);
    instr.addOperand(src);
    return true;
}

} // namespace VBDecompiler
