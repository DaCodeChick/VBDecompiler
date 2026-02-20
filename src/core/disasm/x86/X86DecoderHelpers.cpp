// X86DecoderHelpers.cpp - Helper functions for x86 instruction decoding
// Part of X86Disassembler RDSS refactoring

#include "X86Disassembler.h"

namespace VBDecompiler {

// ============================================================================
// Data Reading Helpers
// ============================================================================

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

// ============================================================================
// Register Lookup Helpers
// ============================================================================

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

// ============================================================================
// ModR/M and SIB Decoding
// ============================================================================

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

// ============================================================================
// Memory Operand Decoding
// ============================================================================

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

} // namespace VBDecompiler
