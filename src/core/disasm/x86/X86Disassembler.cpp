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

// Decode ModR/M byte
bool X86Disassembler::decodeModRM(std::span<const uint8_t> data, size_t& offset,
                                   uint8_t& mod, uint8_t& reg, uint8_t& rm) {
    uint8_t modrm;
    if (!readByte(data, offset, modrm)) return false;
    
    mod = (modrm >> 6) & 0x03;  // Bits 7-6
    reg = (modrm >> 3) & 0x07;  // Bits 5-3
    rm  = modrm & 0x07;         // Bits 2-0
    
    return true;
}

// Decode SIB byte
bool X86Disassembler::decodeSIB(std::span<const uint8_t> data, size_t& offset,
                                 uint8_t& scale, uint8_t& index, uint8_t& base) {
    uint8_t sib;
    if (!readByte(data, offset, sib)) return false;
    
    scale = (sib >> 6) & 0x03;  // Bits 7-6 (scale: 1, 2, 4, 8)
    index = (sib >> 3) & 0x07;  // Bits 5-3
    base  = sib & 0x07;         // Bits 2-0
    
    return true;
}

// Decode memory operand from ModR/M byte
bool X86Disassembler::decodeMemoryOperand(std::span<const uint8_t> data, size_t& offset,
                                           uint8_t mod, uint8_t rm, uint8_t operandSize,
                                           X86Operand& operand) {
    operand.type = X86OperandType::MEMORY;
    operand.size = operandSize;
    
    // mod = 11b means register, not memory (shouldn't be called for this case)
    if (mod == 3) {
        setError("decodeMemoryOperand called with mod=3 (register)");
        return false;
    }
    
    // Check for SIB byte (rm = 100b and mod != 11b)
    bool hasSIB = (rm == 4);
    
    // mod = 00b: [reg] or [SIB] or disp32
    if (mod == 0) {
        if (rm == 5) {
            // [disp32] - absolute addressing
            uint32_t disp;
            if (!readDword(data, offset, disp)) return false;
            operand.displacement = static_cast<int32_t>(disp);
        }
        else if (hasSIB) {
            // [SIB]
            uint8_t scale, index, base;
            if (!decodeSIB(data, offset, scale, index, base)) return false;
            
            if (base == 5) {
                // [index*scale + disp32]
                uint32_t disp;
                if (!readDword(data, offset, disp)) return false;
                operand.displacement = static_cast<int32_t>(disp);
            } else {
                operand.base = getReg32(base);
            }
            
            if (index != 4) {  // ESP can't be index
                operand.index = getReg32(index);
                operand.scale = 1 << scale;  // Convert to 1, 2, 4, 8
            }
        }
        else {
            // [reg]
            operand.base = getReg32(rm);
        }
    }
    // mod = 01b: [reg + disp8] or [SIB + disp8]
    else if (mod == 1) {
        int8_t disp;
        if (!readSByte(data, offset, disp)) return false;
        operand.displacement = disp;
        
        if (hasSIB) {
            uint8_t scale, index, base;
            if (!decodeSIB(data, offset, scale, index, base)) return false;
            
            operand.base = getReg32(base);
            if (index != 4) {
                operand.index = getReg32(index);
                operand.scale = 1 << scale;
            }
        }
        else {
            operand.base = getReg32(rm);
        }
    }
    // mod = 10b: [reg + disp32] or [SIB + disp32]
    else if (mod == 2) {
        int32_t disp;
        if (!readSDword(data, offset, disp)) return false;
        operand.displacement = disp;
        
        if (hasSIB) {
            uint8_t scale, index, base;
            if (!decodeSIB(data, offset, scale, index, base)) return false;
            
            operand.base = getReg32(base);
            if (index != 4) {
                operand.index = getReg32(index);
                operand.scale = 1 << scale;
            }
        }
        else {
            operand.base = getReg32(rm);
        }
    }
    
    return true;
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

// Decode MOV instruction
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

// Decode INC/DEC instructions (Increment/Decrement)
bool X86Disassembler::decodeIncDec(std::span<const uint8_t> data, size_t& offset,
                                    X86Instruction& instr, uint8_t opcode) {
    
    if (opcode >= 0x40 && opcode <= 0x47) {
        // INC r32 (0x40-0x47)
        instr.setOpcode(X86Opcode::INC);
        
        X86Operand operand;
        operand.type = X86OperandType::REGISTER;
        operand.reg = getReg32(opcode - 0x40);
        operand.size = 4;
        instr.addOperand(operand);
    }
    else if (opcode >= 0x48 && opcode <= 0x4F) {
        // DEC r32 (0x48-0x4F)
        instr.setOpcode(X86Opcode::DEC);
        
        X86Operand operand;
        operand.type = X86OperandType::REGISTER;
        operand.reg = getReg32(opcode - 0x48);
        operand.size = 4;
        instr.addOperand(operand);
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
            operand.type = X86OperandType::REGISTER;
            operand.reg = getReg8(rm);
            operand.size = 1;
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
            operand.type = X86OperandType::REGISTER;
            operand.reg = getReg32(rm);
            operand.size = 4;
        } else {
            if (!decodeMemoryOperand(data, offset, mod, rm, 4, operand)) return false;
        }
        instr.addOperand(operand);
    }
    
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
