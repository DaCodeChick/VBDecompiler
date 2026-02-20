// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "PCodeDisassembler.h"
#include <cstring>

namespace VBDecompiler {

// ============================================================================
// Public API
// ============================================================================

std::unique_ptr<PCodeInstruction> PCodeDisassembler::disassembleOne(
    std::span<const uint8_t> data,
    size_t& offset,
    uint32_t address,
    const VBPublicObjectDescriptor* objectInfo) {
    
    if (offset >= data.size()) {
        setError("Offset out of bounds");
        return nullptr;
    }
    
    objectInfo_ = objectInfo;
    
    auto instr = std::make_unique<PCodeInstruction>();
    instr->setAddress(address);
    
    size_t startOffset = offset;
    
    // Decode the instruction
    if (!decodeInstruction(data, offset, *instr)) {
        return nullptr;
    }
    
    // Set instruction length
    instr->setLength(static_cast<uint8_t>(offset - startOffset));
    
    // Copy raw bytes
    std::vector<uint8_t> bytes(data.begin() + startOffset, data.begin() + offset);
    instr->setBytes(bytes);
    
    return instr;
}

std::vector<PCodeInstruction> PCodeDisassembler::disassemble(
    std::span<const uint8_t> data,
    size_t startOffset,
    uint32_t address,
    size_t count,
    const VBPublicObjectDescriptor* objectInfo) {
    
    std::vector<PCodeInstruction> instructions;
    size_t offset = startOffset;
    size_t instructionCount = 0;
    
    while (offset < data.size() && (count == 0 || instructionCount < count)) {
        auto instr = disassembleOne(data, offset, address, objectInfo);
        if (!instr) break;
        
        address += instr->getLength();
        instructions.push_back(*instr);
        ++instructionCount;
    }
    
    return instructions;
}

std::vector<PCodeInstruction> PCodeDisassembler::disassembleProcedure(
    std::span<const uint8_t> data,
    size_t startOffset,
    uint32_t address,
    const VBPublicObjectDescriptor* objectInfo) {
    
    std::vector<PCodeInstruction> instructions;
    size_t offset = startOffset;
    
    while (offset < data.size()) {
        auto instr = disassembleOne(data, offset, address, objectInfo);
        if (!instr) break;
        
        address += instr->getLength();
        instructions.push_back(*instr);
        
        // Stop at procedure exit (ExitProc or ExitProcHresult)
        if (instr->getMnemonic() == "ExitProc" || instr->getMnemonic() == "ExitProcHresult") {
            break;
        }
    }
    
    return instructions;
}

// ============================================================================
// Core Decoding
// ============================================================================

bool PCodeDisassembler::decodeInstruction(
    std::span<const uint8_t> data,
    size_t& offset,
    PCodeInstruction& instr) {
    
    // Read primary opcode
    uint8_t opcode;
    if (!readByte(data, offset, opcode)) {
        setError("Failed to read opcode");
        return false;
    }
    
    // Check if extended opcode
    if (isExtendedOpcode(opcode)) {
        uint8_t extOpcode;
        if (!readByte(data, offset, extOpcode)) {
            setError("Failed to read extended opcode");
            return false;
        }
        
        // Get extended opcode info
        const PCodeOpcodeInfo* opcodeInfo = getExtendedOpcodeInfo(opcode, extOpcode);
        if (!opcodeInfo) {
            // Unknown extended opcode
            instr.setOpcode(opcode);
            instr.setExtendedOpcode(extOpcode);
            instr.setMnemonic("Unknown");
            instr.setCategory(PCodeOpcodeCategory::UNKNOWN);
            return true;
        }
        
        instr.setOpcode(opcodeInfo->opcode);
        instr.setExtendedOpcode(opcodeInfo->extOpcode);
        instr.setMnemonic(std::string(opcodeInfo->mnemonic));
        instr.setCategory(opcodeInfo->category);
        instr.setStackDelta(opcodeInfo->stackDelta);
        
        // Decode operands based on format string
        return decodeOperands(data, offset, opcodeInfo->format, instr);
    }
    
    // Standard opcode
    const PCodeOpcodeInfo* opcodeInfo = getOpcodeInfo(opcode);
    if (!opcodeInfo) {
        setError("Invalid opcode");
        return false;
    }
    
    instr.setOpcode(opcodeInfo->opcode);
    instr.setExtendedOpcode(0);
    instr.setMnemonic(std::string(opcodeInfo->mnemonic));
    instr.setCategory(opcodeInfo->category);
    instr.setStackDelta(opcodeInfo->stackDelta);
    
    // Decode operands based on format string
    return decodeOperands(data, offset, opcodeInfo->format, instr);
}

bool PCodeDisassembler::decodeOperands(
    std::span<const uint8_t> data,
    size_t& offset,
    std::string_view format,
    PCodeInstruction& instr) {
    
    // Parse format string and decode operands
    // Format characters:
    // b = byte, % = int16, & = int32, ! = float
    // a = argument, c = control, l = local, v = vtable, z = string
    
    for (char ch : format) {
        switch (ch) {
            case 'b':  // Byte operand
                if (!decodeOperandByte(data, offset, instr)) return false;
                break;
                
            case '%':  // Int16 operand
                if (!decodeOperandInt16(data, offset, instr)) return false;
                break;
                
            case '&':  // Int32 operand
                if (!decodeOperandInt32(data, offset, instr)) return false;
                break;
                
            case '!':  // Float operand
                if (!decodeOperandFloat(data, offset, instr)) return false;
                break;
                
            case 'a':  // Argument reference
                if (!decodeOperandArgument(data, offset, instr)) return false;
                break;
                
            case 'c':  // Control index
                if (!decodeOperandControl(data, offset, instr)) return false;
                break;
                
            case 'l':  // Local variable
                if (!decodeOperandLocal(data, offset, instr)) return false;
                break;
                
            case 'z':  // String
                if (!decodeOperandString(data, offset, instr)) return false;
                break;
                
            case 'v':  // VTable reference
                if (!decodeOperandVTable(data, offset, instr)) return false;
                break;
                
            // Type characters following operands - just skip them
            case '?':  // Boolean type
            case '~':  // Variant type
                // These are type annotations, not separate operands
                break;
                
            default:
                // Unknown format character - ignore
                break;
        }
    }
    
    return true;
}

// ============================================================================
// Operand Decoders
// ============================================================================

bool PCodeDisassembler::decodeOperandByte(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    uint8_t value;
    if (!readByte(data, offset, value)) {
        setError("Failed to read byte operand");
        return false;
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::BYTE,
        value,
        PCodeType::BYTE
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandInt16(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    int16_t value;
    if (!readInt16(data, offset, value)) {
        setError("Failed to read int16 operand");
        return false;
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::INT16,
        value,
        PCodeType::INTEGER
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandInt32(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    int32_t value;
    if (!readInt32(data, offset, value)) {
        setError("Failed to read int32 operand");
        return false;
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::INT32,
        value,
        PCodeType::LONG
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandFloat(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    float value;
    if (!readFloat(data, offset, value)) {
        setError("Failed to read float operand");
        return false;
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::FLOAT,
        value,
        PCodeType::SINGLE
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandString(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    std::string value;
    if (!readString(data, offset, value)) {
        setError("Failed to read string operand");
        return false;
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::STRING,
        value,
        PCodeType::STRING
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandArgument(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    int16_t index;
    if (!readInt16(data, offset, index)) {
        setError("Failed to read argument index");
        return false;
    }
    
    // Check for type character
    PCodeType dataType = PCodeType::VARIANT;
    if (offset < data.size()) {
        char typeChar = static_cast<char>(data[offset]);
        if (typeChar == '%' || typeChar == '&' || typeChar == '!' || 
            typeChar == '~' || typeChar == 'z' || typeChar == '?') {
            dataType = parseTypeChar(typeChar);
            offset++;
        }
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::ARGUMENT,
        index,
        dataType
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandControl(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    int16_t index;
    if (!readInt16(data, offset, index)) {
        setError("Failed to read control index");
        return false;
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::CONTROL,
        index,
        PCodeType::OBJECT
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandLocal(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    int16_t index;
    if (!readInt16(data, offset, index)) {
        setError("Failed to read local variable index");
        return false;
    }
    
    // Check for type character
    PCodeType dataType = PCodeType::VARIANT;
    if (offset < data.size()) {
        char typeChar = static_cast<char>(data[offset]);
        if (typeChar == '%' || typeChar == '&' || typeChar == '!' || 
            typeChar == '~' || typeChar == 'z' || typeChar == '?') {
            dataType = parseTypeChar(typeChar);
            offset++;
        }
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::LOCAL_VAR,
        index,
        dataType
    ));
    
    return true;
}

bool PCodeDisassembler::decodeOperandVTable(
    std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr) {
    
    // VTable reference is complex - for now just read as int16
    int16_t index;
    if (!readInt16(data, offset, index)) {
        setError("Failed to read vtable reference");
        return false;
    }
    
    instr.addOperand(PCodeOperand(
        PCodeOperandType::VTABLE,
        index,
        PCodeType::OBJECT
    ));
    
    return true;
}

// ============================================================================
// Read Helpers
// ============================================================================

bool PCodeDisassembler::readByte(
    std::span<const uint8_t> data, size_t& offset, uint8_t& value) {
    
    if (offset >= data.size()) {
        setError("Unexpected end of data reading byte");
        return false;
    }
    
    value = data[offset++];
    return true;
}

bool PCodeDisassembler::readInt16(
    std::span<const uint8_t> data, size_t& offset, int16_t& value) {
    
    if (offset + 1 >= data.size()) {
        setError("Unexpected end of data reading int16");
        return false;
    }
    
    // Little-endian
    value = static_cast<int16_t>(
        data[offset] | (data[offset + 1] << 8)
    );
    offset += 2;
    return true;
}

bool PCodeDisassembler::readInt32(
    std::span<const uint8_t> data, size_t& offset, int32_t& value) {
    
    if (offset + 3 >= data.size()) {
        setError("Unexpected end of data reading int32");
        return false;
    }
    
    // Little-endian
    value = static_cast<int32_t>(
        data[offset] | 
        (data[offset + 1] << 8) | 
        (data[offset + 2] << 16) | 
        (data[offset + 3] << 24)
    );
    offset += 4;
    return true;
}

bool PCodeDisassembler::readFloat(
    std::span<const uint8_t> data, size_t& offset, float& value) {
    
    if (offset + 3 >= data.size()) {
        setError("Unexpected end of data reading float");
        return false;
    }
    
    // Read as int32 then reinterpret as float
    uint32_t bits = static_cast<uint32_t>(
        data[offset] | 
        (data[offset + 1] << 8) | 
        (data[offset + 2] << 16) | 
        (data[offset + 3] << 24)
    );
    offset += 4;
    
    std::memcpy(&value, &bits, sizeof(float));
    return true;
}

bool PCodeDisassembler::readString(
    std::span<const uint8_t> data, size_t& offset, std::string& value) {
    
    // P-Code strings are null-terminated Unicode (UTF-16LE)
    value.clear();
    
    while (offset + 1 < data.size()) {
        uint16_t wchar = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        
        if (wchar == 0) {
            break;  // Null terminator
        }
        
        // Simple conversion: just take low byte for ASCII range
        // TODO: Proper UTF-16 to UTF-8 conversion
        if (wchar < 128) {
            value += static_cast<char>(wchar);
        } else {
            value += '?';  // Placeholder for non-ASCII
        }
    }
    
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

void PCodeDisassembler::setError(const std::string& error) {
    lastError_ = error;
}

} // namespace VBDecompiler
