// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#pragma once

#include "PCodeInstruction.h"
#include "PCodeOpcode.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace VBDecompiler {

// Forward declaration of VB structures
struct VBPublicObjectDescriptor;

/**
 * @brief P-Code Disassembler
 * 
 * Decodes Visual Basic P-Code (bytecode) into PCodeInstruction objects.
 * Supports VB5/VB6 P-Code format with standard and extended opcodes.
 * 
 * P-Code is a stack-based bytecode format with variable-length instructions.
 * Each instruction consists of an opcode byte followed by 0-4 operand bytes.
 * Extended opcodes (0xFB-0xFF) are followed by a second opcode byte.
 */
class PCodeDisassembler {
public:
    PCodeDisassembler() = default;
    
    /**
     * @brief Disassemble a single P-Code instruction at the given offset
     * @param data Raw P-Code bytes to disassemble
     * @param offset Current offset in data (updated to next instruction)
     * @param address Virtual address of the instruction (for display)
     * @param objectInfo VB object descriptor (for resolving references)
     * @return Disassembled instruction, or nullptr if decoding failed
     */
    std::unique_ptr<PCodeInstruction> disassembleOne(
        std::span<const uint8_t> data, 
        size_t& offset,
        uint32_t address,
        const VBPublicObjectDescriptor* objectInfo = nullptr);
    
    /**
     * @brief Disassemble multiple P-Code instructions
     * @param data Raw P-Code bytes to disassemble
     * @param startOffset Starting offset in data
     * @param address Starting virtual address
     * @param count Maximum number of instructions to disassemble (0 = all)
     * @param objectInfo VB object descriptor (for resolving references)
     * @return Vector of disassembled instructions
     */
    std::vector<PCodeInstruction> disassemble(
        std::span<const uint8_t> data,
        size_t startOffset,
        uint32_t address,
        size_t count = 0,
        const VBPublicObjectDescriptor* objectInfo = nullptr);
    
    /**
     * @brief Disassemble a P-Code procedure
     * @param data Raw P-Code bytes containing the procedure
     * @param startOffset Starting offset of the procedure
     * @param address Starting address of the procedure
     * @param objectInfo VB object descriptor (for resolving references)
     * @return Vector of disassembled instructions (stops at Exit instruction)
     */
    std::vector<PCodeInstruction> disassembleProcedure(
        std::span<const uint8_t> data,
        size_t startOffset,
        uint32_t address,
        const VBPublicObjectDescriptor* objectInfo = nullptr);
    
    /**
     * @brief Get last error message
     */
    const std::string& getLastError() const { return lastError_; }

private:
    // Core decoding logic
    bool decodeInstruction(
        std::span<const uint8_t> data,
        size_t& offset,
        PCodeInstruction& instr);
    
    // Operand decoding based on format string
    bool decodeOperands(
        std::span<const uint8_t> data,
        size_t& offset,
        std::string_view format,
        PCodeInstruction& instr);
    
    // Individual operand type decoders
    bool decodeOperandByte(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandInt16(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandInt32(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandFloat(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandString(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandArgument(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandControl(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandLocal(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    bool decodeOperandVTable(std::span<const uint8_t> data, size_t& offset, PCodeInstruction& instr);
    
    // Read helpers
    bool readByte(std::span<const uint8_t> data, size_t& offset, uint8_t& value);
    bool readInt16(std::span<const uint8_t> data, size_t& offset, int16_t& value);
    bool readInt32(std::span<const uint8_t> data, size_t& offset, int32_t& value);
    bool readFloat(std::span<const uint8_t> data, size_t& offset, float& value);
    bool readString(std::span<const uint8_t> data, size_t& offset, std::string& value);
    
    // Utility functions
    void setError(const std::string& error);
    PCodeDataType parseTypeChar(char typeChar);
    
    std::string lastError_;
    const VBPublicObjectDescriptor* objectInfo_ = nullptr;
};

} // namespace VBDecompiler
