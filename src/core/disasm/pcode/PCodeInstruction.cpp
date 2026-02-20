// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "PCodeInstruction.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace VBDecompiler {

// PCodeOperand implementation

std::string PCodeOperand::toString() const {
    std::ostringstream oss;
    
    switch (type) {
        case PCodeOperandType::NONE:
            return "";
            
        case PCodeOperandType::BYTE:
            oss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                << static_cast<int>(getByte());
            break;
            
        case PCodeOperandType::INT16:
            oss << getInt16();
            break;
            
        case PCodeOperandType::INT32:
        case PCodeOperandType::ADDRESS:
            oss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                << static_cast<uint32_t>(getInt32());
            break;
            
        case PCodeOperandType::FLOAT:
            oss << getFloat();
            break;
            
        case PCodeOperandType::STRING:
            oss << "\"" << getString() << "\"";
            break;
            
        case PCodeOperandType::LOCAL_VAR:
            oss << "local_" << getInt16();
            break;
            
        case PCodeOperandType::ARGUMENT:
            oss << "arg_" << getInt16();
            break;
            
        case PCodeOperandType::CONTROL:
            oss << "ctrl_" << getInt16();
            break;
            
        case PCodeOperandType::BRANCH_OFFSET:
            {
                int32_t offset = getInt32();
                if (offset >= 0) {
                    oss << "+0x" << std::hex << std::uppercase << offset;
                } else {
                    oss << "-0x" << std::hex << std::uppercase << (-offset);
                }
            }
            break;
            
        case PCodeOperandType::VTABLE:
            oss << "vtable_" << std::hex << std::uppercase << getInt32();
            break;
    }
    
    // Add type suffix if present
    if (dataType != PCodeType::UNKNOWN) {
        oss << " [" << pCodeTypeToString(dataType) << "]";
    }
    
    return oss.str();
}

bool PCodeOperand::hasValue() const {
    return !std::holds_alternative<std::monostate>(value);
}

uint8_t PCodeOperand::getByte() const {
    if (auto* val = std::get_if<uint8_t>(&value)) {
        return *val;
    }
    throw std::runtime_error("Operand is not a byte");
}

int16_t PCodeOperand::getInt16() const {
    if (auto* val = std::get_if<int16_t>(&value)) {
        return *val;
    }
    throw std::runtime_error("Operand is not an int16");
}

int32_t PCodeOperand::getInt32() const {
    if (auto* val = std::get_if<int32_t>(&value)) {
        return *val;
    }
    throw std::runtime_error("Operand is not an int32");
}

float PCodeOperand::getFloat() const {
    if (auto* val = std::get_if<float>(&value)) {
        return *val;
    }
    throw std::runtime_error("Operand is not a float");
}

std::string PCodeOperand::getString() const {
    if (auto* val = std::get_if<std::string>(&value)) {
        return *val;
    }
    throw std::runtime_error("Operand is not a string");
}

// PCodeInstruction implementation

std::string PCodeInstruction::toString() const {
    std::ostringstream oss;
    
    // Format: "MNEMONIC operand1, operand2, ..."
    oss << mnemonic_;
    
    if (!operands_.empty()) {
        oss << " ";
        for (size_t i = 0; i < operands_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << operands_[i].toString();
        }
    }
    
    return oss.str();
}

std::string PCodeInstruction::bytesToHex() const {
    std::ostringstream oss;
    for (size_t i = 0; i < bytes_.size(); ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(bytes_[i]);
    }
    return oss.str();
}

// Helper functions

std::string pCodeTypeToString(PCodeType type) {
    switch (type) {
        case PCodeType::UNKNOWN:   return "?";
        case PCodeType::BYTE:      return "Byte";
        case PCodeType::BOOLEAN:   return "Boolean";
        case PCodeType::INTEGER:   return "Integer";
        case PCodeType::LONG:      return "Long";
        case PCodeType::SINGLE:    return "Single";
        case PCodeType::VARIANT:   return "Variant";
        case PCodeType::STRING:    return "String";
        case PCodeType::OBJECT:    return "Object";
        default:                   return "Unknown";
    }
}

PCodeType parseTypeChar(char typeChar) {
    switch (typeChar) {
        case 'b': return PCodeType::BYTE;
        case '?': return PCodeType::BOOLEAN;
        case '%': return PCodeType::INTEGER;
        case '&': return PCodeType::LONG;
        case '!': return PCodeType::SINGLE;
        case '~': return PCodeType::VARIANT;
        case 'z': return PCodeType::STRING;
        case 'o': return PCodeType::OBJECT;
        default:  return PCodeType::UNKNOWN;
    }
}

} // namespace VBDecompiler
