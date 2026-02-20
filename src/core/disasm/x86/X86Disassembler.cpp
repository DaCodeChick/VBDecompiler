// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "X86Disassembler.h"
#include <cstring>

namespace VBDecompiler {

// Main disassembly entry point - decodes one instruction
std::unique_ptr<X86Instruction> X86Disassembler::disassembleOne(
    std::span<const uint8_t> data, uint32_t address) {
    
    if (data.empty()) {
        setError("Empty data");
        return nullptr;
    }
    
    auto instr = std::make_unique<X86Instruction>();
    instr->setAddress(address);
    
    size_t offset = 0;
    uint8_t opcode;
    
    if (!readByte(data, offset, opcode)) {
        return nullptr;
    }
    
    // Store first byte
    std::vector<uint8_t> bytes = {opcode};
    
    // Decode based on opcode
    bool decoded = false;
    
    // MOV instructions (0x88-0x8B, 0xA0-0xA3, 0xB0-0xBF, 0xC6-0xC7)
    if ((opcode >= 0x88 && opcode <= 0x8B) ||
        (opcode >= 0xA0 && opcode <= 0xA3) ||
        (opcode >= 0xB0 && opcode <= 0xBF) ||
        (opcode >= 0xC6 && opcode <= 0xC7)) {
        decoded = decodeMov(data, offset, *instr, opcode);
    }
    // PUSH register (0x50-0x57)
    else if (opcode >= 0x50 && opcode <= 0x57) {
        decoded = decodePush(data, offset, *instr, opcode);
    }
    // PUSH immediate (0x68 = imm32, 0x6A = imm8)
    else if (opcode == 0x68 || opcode == 0x6A) {
        decoded = decodePush(data, offset, *instr, opcode);
    }
    // POP register (0x58-0x5F)
    else if (opcode >= 0x58 && opcode <= 0x5F) {
        decoded = decodePop(data, offset, *instr, opcode);
    }
    // CALL (0xE8 = near relative, 0xFF /2 = indirect)
    else if (opcode == 0xE8 || opcode == 0xFF) {
        decoded = decodeCall(data, offset, *instr, opcode);
    }
    // JMP (0xE9 = near, 0xEB = short, 0xFF /4 = indirect)
    else if (opcode == 0xE9 || opcode == 0xEB) {
        decoded = decodeJmp(data, offset, *instr, opcode);
    }
    // Conditional jumps (0x70-0x7F = short, 0x0F 0x80-0x8F = near)
    else if ((opcode >= 0x70 && opcode <= 0x7F) || opcode == 0x0F) {
        decoded = decodeJcc(data, offset, *instr, opcode);
    }
    // RET (0xC3 = near, 0xC2 = near with pop, 0xCB/0xCA = far)
    else if (opcode == 0xC3 || opcode == 0xC2 || opcode == 0xCB || opcode == 0xCA) {
        decoded = decodeRet(data, offset, *instr, opcode);
    }
    // LEA (0x8D)
    else if (opcode == 0x8D) {
        decoded = decodeLea(data, offset, *instr, opcode);
    }
    // TEST (0x84, 0x85, 0xA8, 0xA9, 0xF6 /0, 0xF7 /0)
    else if (opcode == 0x84 || opcode == 0x85 || opcode == 0xA8 || opcode == 0xA9) {
        decoded = decodeTest(data, offset, *instr, opcode);
    }
    // XOR (0x30-0x35)
    else if (opcode >= 0x30 && opcode <= 0x35) {
        decoded = decodeXor(data, offset, *instr, opcode);
    }
    // AND (0x20-0x25)
    else if (opcode >= 0x20 && opcode <= 0x25) {
        decoded = decodeAnd(data, offset, *instr, opcode);
    }
    // OR (0x08-0x0D)
    else if (opcode >= 0x08 && opcode <= 0x0D) {
        decoded = decodeOr(data, offset, *instr, opcode);
    }
    // INC/DEC register (0x40-0x4F)
    else if (opcode >= 0x40 && opcode <= 0x4F) {
        decoded = decodeIncDec(data, offset, *instr, opcode);
    }
    // LEAVE (0xC9)
    else if (opcode == 0xC9) {
        decoded = decodeLeave(data, offset, *instr);
    }
    // NOP (0x90)
    else if (opcode == 0x90) {
        decoded = decodeNop(data, offset, *instr);
    }
    // ADD, SUB, CMP, etc (0x00-0x3D various)
    else if ((opcode >= 0x00 && opcode <= 0x05) ||  // ADD
             (opcode >= 0x28 && opcode <= 0x2D) ||  // SUB
             (opcode >= 0x38 && opcode <= 0x3D)) {  // CMP
        decoded = decodeArithmetic(data, offset, *instr, opcode);
    }
    
    if (!decoded) {
        instr->setOpcode(X86Opcode::UNKNOWN);
        instr->setLength(1);
        instr->setBytes({opcode});
        return instr;
    }
    
    // Collect all bytes
    for (size_t i = 1; i < offset; ++i) {
        bytes.push_back(data[i]);
    }
    
    instr->setLength(static_cast<uint8_t>(offset));
    instr->setBytes(bytes);
    
    return instr;
}

// Disassemble multiple instructions
std::vector<X86Instruction> X86Disassembler::disassemble(
    std::span<const uint8_t> data, uint32_t address, size_t count) {
    
    std::vector<X86Instruction> instructions;
    size_t offset = 0;
    size_t instructionCount = 0;
    
    while (offset < data.size() && (count == 0 || instructionCount < count)) {
        auto instr = disassembleOne(data.subspan(offset), address + offset);
        if (!instr) break;
        
        offset += instr->getLength();
        instructions.push_back(*instr);
        ++instructionCount;
    }
    
    return instructions;
}

// Disassemble an entire function (until RET)
std::vector<X86Instruction> X86Disassembler::disassembleFunction(
    std::span<const uint8_t> data, uint32_t address) {
    
    std::vector<X86Instruction> instructions;
    size_t offset = 0;
    
    while (offset < data.size()) {
        auto instr = disassembleOne(data.subspan(offset), address + offset);
        if (!instr) break;
        
        offset += instr->getLength();
        instructions.push_back(*instr);
        
        // Stop at RET
        if (instr->isReturn()) {
            break;
        }
    }
    
    return instructions;
}

// Set error message
void X86Disassembler::setError(const std::string& error) {
    lastError_ = error;
}

} // namespace VBDecompiler
