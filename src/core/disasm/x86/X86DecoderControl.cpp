// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "X86Disassembler.h"
#include <cstdint>

namespace VBDecompiler {

// Decode CALL instruction
bool X86Disassembler::decodeCall(std::span<const uint8_t> data, size_t& offset,
                                  X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::CALL);
    
    if (opcode == 0xE8) {
        // CALL rel32 - near relative call
        int32_t relOffset;
        if (!readSDword(data, offset, relOffset)) return false;
        instr.addOperand(makeOffsetOperand(relOffset, 4));
    }
    
    return true;
}

// Decode JMP instruction
bool X86Disassembler::decodeJmp(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::JMP);
    
    if (opcode == 0xE9) {
        // JMP rel32 - near jump
        int32_t relOffset;
        if (!readSDword(data, offset, relOffset)) return false;
        instr.addOperand(makeOffsetOperand(relOffset, 4));
    }
    else if (opcode == 0xEB) {
        // JMP rel8 - short jump
        int8_t relOffset;
        if (!readSByte(data, offset, relOffset)) return false;
        instr.addOperand(makeOffsetOperand(relOffset, 1));
    }
    
    return true;
}

// Decode conditional jump
bool X86Disassembler::decodeJcc(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    
    // Short conditional jumps (0x70-0x7F)
    if (opcode >= 0x70 && opcode <= 0x7F) {
        // Map opcode to condition
        static const X86Opcode jccOpcodes[] = {
            X86Opcode::JO, X86Opcode::JNO, X86Opcode::JB, X86Opcode::JAE,
            X86Opcode::JE, X86Opcode::JNE, X86Opcode::JBE, X86Opcode::JA,
            X86Opcode::JS, X86Opcode::JNS, X86Opcode::JP, X86Opcode::JNP,
            X86Opcode::JL, X86Opcode::JGE, X86Opcode::JLE, X86Opcode::JG
        };
        
        instr.setOpcode(jccOpcodes[opcode & 0x0F]);
        
        int8_t relOffset;
        if (!readSByte(data, offset, relOffset)) return false;
        instr.addOperand(makeOffsetOperand(relOffset, 1));
        
        return true;
    }
    
    // Near conditional jumps (0x0F 0x80-0x8F) - would need second byte
    // TODO: Implement 0x0F prefix handling
    
    return false;
}

// Decode RET instruction
bool X86Disassembler::decodeRet(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    
    if (opcode == 0xC3) {
        // RET near
        instr.setOpcode(X86Opcode::RET);
    }
    else if (opcode == 0xC2) {
        // RETN imm16 - return and pop imm16 bytes
        instr.setOpcode(X86Opcode::RETN);
        
        uint16_t popBytes;
        if (!readWord(data, offset, popBytes)) return false;
        instr.addOperand(makeImmediateOperand(popBytes, 2));
    }
    else if (opcode == 0xCB || opcode == 0xCA) {
        // RETF - far return
        instr.setOpcode(X86Opcode::RETF);
    }
    
    return true;
}

// Decode LEAVE instruction
bool X86Disassembler::decodeLeave(std::span<const uint8_t> data, size_t& offset,
                                   X86Instruction& instr) {
    (void)data;    // Unused
    (void)offset;  // Unused
    instr.setOpcode(X86Opcode::LEAVE);
    return true;
}

// Decode NOP instruction
bool X86Disassembler::decodeNop(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr) {
    (void)data;    // Unused
    (void)offset;  // Unused
    instr.setOpcode(X86Opcode::NOP);
    return true;
}

} // namespace VBDecompiler
