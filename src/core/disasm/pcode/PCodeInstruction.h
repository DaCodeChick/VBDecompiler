// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <variant>

namespace VBDecompiler {

// Forward declaration
enum class PCodeOpcodeCategory;

/// P-Code operand types
enum class PCodeOperandType {
    NONE,           // No operand
    BYTE,           // 8-bit value
    INT16,          // 16-bit integer
    INT32,          // 32-bit integer (Long)
    FLOAT,          // 32-bit float (Single)
    STRING,         // String literal
    LOCAL_VAR,      // Local variable reference
    ARGUMENT,       // Argument reference
    CONTROL,        // Control index
    BRANCH_OFFSET,  // Branch offset (relative)
    ADDRESS,        // Absolute address
    VTABLE          // VTable reference
};

/// P-Code data type specifier
enum class PCodeType {
    UNKNOWN,
    BYTE,       // b
    BOOLEAN,    // ?
    INTEGER,    // % (2 bytes)
    LONG,       // & (4 bytes)
    SINGLE,     // ! (4 bytes float)
    VARIANT,    // ~ (Variant type)
    STRING,     // z (String)
    OBJECT      // Object reference
};

/// P-Code operand value (variant)
using PCodeOperandValue = std::variant<
    std::monostate,  // No value
    uint8_t,         // Byte
    int16_t,         // Int16
    int32_t,         // Int32
    float,           // Float
    std::string      // String
>;

/// Single P-Code operand
struct PCodeOperand {
    PCodeOperandType type = PCodeOperandType::NONE;
    PCodeOperandValue value;
    PCodeType dataType = PCodeType::UNKNOWN;
    
    PCodeOperand() = default;
    
    PCodeOperand(PCodeOperandType t, PCodeOperandValue v, 
                 PCodeType dt = PCodeType::UNKNOWN)
        : type(t), value(std::move(v)), dataType(dt) {}
    
    /// Get operand as string for display
    std::string toString() const;
    
    /// Check if operand has a value
    bool hasValue() const;
    
    /// Get byte value (if type is BYTE)
    uint8_t getByte() const;
    
    /// Get int16 value (if type is INT16)
    int16_t getInt16() const;
    
    /// Get int32 value (if type is INT32)
    int32_t getInt32() const;
    
    /// Get float value (if type is FLOAT)
    float getFloat() const;
    
    /// Get string value (if type is STRING)
    std::string getString() const;
};

/// P-Code instruction representation
class PCodeInstruction {
public:
    PCodeInstruction() = default;
    
    /// Get instruction address
    uint32_t getAddress() const { return address_; }
    void setAddress(uint32_t addr) { address_ = addr; }
    
    /// Get instruction length in bytes
    uint8_t getLength() const { return length_; }
    void setLength(uint8_t len) { length_ = len; }
    
    /// Get primary opcode byte
    uint8_t getOpcode() const { return opcode_; }
    void setOpcode(uint8_t op) { opcode_ = op; }
    
    /// Get secondary opcode (for extended opcodes 0xFB-0xFF)
    uint8_t getExtendedOpcode() const { return extOpcode_; }
    void setExtendedOpcode(uint8_t op) { extOpcode_ = op; }
    
    /// Check if this is an extended opcode (0xFB-0xFF)
    bool isExtended() const { return opcode_ >= 0xFB && opcode_ <= 0xFF; }
    
    /// Get mnemonic string
    const std::string& getMnemonic() const { return mnemonic_; }
    void setMnemonic(const std::string& mn) { mnemonic_ = mn; }
    
    /// Get operands
    const std::vector<PCodeOperand>& getOperands() const { return operands_; }
    void addOperand(const PCodeOperand& op) { operands_.push_back(op); }
    void clearOperands() { operands_.clear(); }
    
    /// Get raw instruction bytes
    const std::vector<uint8_t>& getBytes() const { return bytes_; }
    void setBytes(const std::vector<uint8_t>& b) { bytes_ = b; }
    
    /// Get stack delta (change in stack depth: +1 = push, -1 = pop)
    int getStackDelta() const { return stackDelta_; }
    void setStackDelta(int delta) { stackDelta_ = delta; }
    
    /// Get opcode category
    PCodeOpcodeCategory getCategory() const { return category_; }
    void setCategory(PCodeOpcodeCategory cat) { category_ = cat; }
    
    /// Control flow properties
    bool isBranch() const { return isBranch_; }
    void setIsBranch(bool b) { isBranch_ = b; }
    
    bool isConditionalBranch() const { return isConditionalBranch_; }
    void setIsConditionalBranch(bool b) { isConditionalBranch_ = b; }
    
    bool isCall() const { return isCall_; }
    void setIsCall(bool c) { isCall_ = c; }
    
    bool isReturn() const { return isReturn_; }
    void setIsReturn(bool r) { isReturn_ = r; }
    
    /// Get branch target (for branch instructions)
    int32_t getBranchOffset() const { return branchOffset_; }
    void setBranchOffset(int32_t offset) { branchOffset_ = offset; }
    
    /// Format instruction to string (assembly-like format)
    std::string toString() const;
    
    /// Format bytes as hex string
    std::string bytesToHex() const;

private:
    uint32_t address_ = 0;          // Instruction address
    uint8_t length_ = 0;            // Instruction length in bytes
    uint8_t opcode_ = 0;            // Primary opcode
    uint8_t extOpcode_ = 0;         // Extended opcode (if isExtended())
    std::string mnemonic_;          // Instruction mnemonic
    std::vector<PCodeOperand> operands_;  // Operands
    std::vector<uint8_t> bytes_;    // Raw bytes
    PCodeOpcodeCategory category_;  // Opcode category
    
    // Stack tracking
    int stackDelta_ = 0;            // Stack depth change
    
    // Control flow
    bool isBranch_ = false;
    bool isConditionalBranch_ = false;
    bool isCall_ = false;
    bool isReturn_ = false;
    int32_t branchOffset_ = 0;      // Branch target offset
};

/// Get type character string
std::string pCodeTypeToString(PCodeType type);

/// Parse type character to enum
PCodeType parseTypeChar(char typeChar);

} // namespace VBDecompiler
