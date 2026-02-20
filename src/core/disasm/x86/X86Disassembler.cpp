#include "X86Disassembler.h"
#include <cstring>

namespace VBDecompiler {

// Helper: Read unsigned byte
bool X86Disassembler::readByte(std::span<const uint8_t> data, size_t& offset, uint8_t& value) {
    if (offset >= data.size()) {
        setError("Unexpected end of data");
        return false;
    }
    value = data[offset++];
    return true;
}

// Helper: Read unsigned word (16-bit)
bool X86Disassembler::readWord(std::span<const uint8_t> data, size_t& offset, uint16_t& value) {
    if (offset + 2 > data.size()) {
        setError("Unexpected end of data");
        return false;
    }
    value = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    return true;
}

// Helper: Read unsigned dword (32-bit)
bool X86Disassembler::readDword(std::span<const uint8_t> data, size_t& offset, uint32_t& value) {
    if (offset + 4 > data.size()) {
        setError("Unexpected end of data");
        return false;
    }
    value = data[offset] | (data[offset + 1] << 8) | 
            (data[offset + 2] << 16) | (data[offset + 3] << 24);
    offset += 4;
    return true;
}

// Helper: Read signed byte
bool X86Disassembler::readSByte(std::span<const uint8_t> data, size_t& offset, int8_t& value) {
    uint8_t uvalue;
    if (!readByte(data, offset, uvalue)) return false;
    value = static_cast<int8_t>(uvalue);
    return true;
}

// Helper: Read signed dword
bool X86Disassembler::readSDword(std::span<const uint8_t> data, size_t& offset, int32_t& value) {
    uint32_t uvalue;
    if (!readDword(data, offset, uvalue)) return false;
    value = static_cast<int32_t>(uvalue);
    return true;
}

// Get 32-bit register from register number
X86Register X86Disassembler::getReg32(uint8_t regNum) {
    static const X86Register regs[] = {
        X86Register::EAX, X86Register::ECX, X86Register::EDX, X86Register::EBX,
        X86Register::ESP, X86Register::EBP, X86Register::ESI, X86Register::EDI
    };
    return (regNum < 8) ? regs[regNum] : X86Register::NONE;
}

// Get 16-bit register from register number
X86Register X86Disassembler::getReg16(uint8_t regNum) {
    static const X86Register regs[] = {
        X86Register::AX, X86Register::CX, X86Register::DX, X86Register::BX,
        X86Register::SP, X86Register::BP, X86Register::SI, X86Register::DI
    };
    return (regNum < 8) ? regs[regNum] : X86Register::NONE;
}

// Get 8-bit register from register number
X86Register X86Disassembler::getReg8(uint8_t regNum) {
    static const X86Register regs[] = {
        X86Register::AL, X86Register::CL, X86Register::DL, X86Register::BL,
        X86Register::AH, X86Register::CH, X86Register::DH, X86Register::BH
    };
    return (regNum < 8) ? regs[regNum] : X86Register::NONE;
}

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
    
    // MOV instructions (0x88-0x8B, 0xB0-0xBF, 0xC6-0xC7)
    if (opcode >= 0x88 && opcode <= 0x8B) {
        decoded = decodeMov(data, offset, *instr, opcode);
    }
    // PUSH register (0x50-0x57)
    else if (opcode >= 0x50 && opcode <= 0x57) {
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

// Decode MOV instruction
bool X86Disassembler::decodeMov(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::MOV);
    
    // MOV r/m8, r8 (0x88)
    // MOV r/m32, r32 (0x89)
    // MOV r8, r/m8 (0x8A)
    // MOV r32, r/m32 (0x8B)
    
    // For now, simple implementation - just mark as MOV
    // Full ModR/M decoding would go here
    
    return true;
}

// Decode PUSH instruction
bool X86Disassembler::decodePush(std::span<const uint8_t> data, size_t& offset,
                                  X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::PUSH);
    
    // PUSH r32 (0x50+r)
    uint8_t regNum = opcode & 0x07;
    X86Operand operand;
    operand.type = X86OperandType::REGISTER;
    operand.reg = getReg32(regNum);
    operand.size = 4;
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

// Decode CALL instruction
bool X86Disassembler::decodeCall(std::span<const uint8_t> data, size_t& offset,
                                  X86Instruction& instr, uint8_t opcode) {
    instr.setOpcode(X86Opcode::CALL);
    
    if (opcode == 0xE8) {
        // CALL rel32 - near relative call
        int32_t relOffset;
        if (!readSDword(data, offset, relOffset)) return false;
        
        X86Operand operand;
        operand.type = X86OperandType::OFFSET;
        operand.offset = relOffset;
        operand.size = 4;
        instr.addOperand(operand);
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
        
        X86Operand operand;
        operand.type = X86OperandType::OFFSET;
        operand.offset = relOffset;
        operand.size = 4;
        instr.addOperand(operand);
    }
    else if (opcode == 0xEB) {
        // JMP rel8 - short jump
        int8_t relOffset;
        if (!readSByte(data, offset, relOffset)) return false;
        
        X86Operand operand;
        operand.type = X86OperandType::OFFSET;
        operand.offset = relOffset;
        operand.size = 1;
        instr.addOperand(operand);
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
        
        X86Operand operand;
        operand.type = X86OperandType::OFFSET;
        operand.offset = relOffset;
        operand.size = 1;
        instr.addOperand(operand);
        
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
        
        X86Operand operand;
        operand.type = X86OperandType::IMMEDIATE;
        operand.immediate = popBytes;
        operand.size = 2;
        instr.addOperand(operand);
    }
    else if (opcode == 0xCB || opcode == 0xCA) {
        // RETF - far return
        instr.setOpcode(X86Opcode::RETF);
    }
    
    return true;
}

// Decode arithmetic instructions
bool X86Disassembler::decodeArithmetic(std::span<const uint8_t> data, size_t& offset,
                                        X86Instruction& instr, uint8_t opcode) {
    // Simplified - map opcode ranges to operation
    if (opcode >= 0x00 && opcode <= 0x05) {
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

// Decode LEAVE instruction
bool X86Disassembler::decodeLeave(std::span<const uint8_t> data, size_t& offset,
                                   X86Instruction& instr) {
    instr.setOpcode(X86Opcode::LEAVE);
    return true;
}

// Decode NOP instruction
bool X86Disassembler::decodeNop(std::span<const uint8_t> data, size_t& offset,
                                 X86Instruction& instr) {
    instr.setOpcode(X86Opcode::NOP);
    return true;
}

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

void X86Disassembler::setError(const std::string& error) {
    lastError_ = error;
}

} // namespace VBDecompiler
