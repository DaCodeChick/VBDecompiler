// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace VBDecompiler {

/// P-Code opcode category
enum class PCodeOpcodeCategory {
    CONTROL_FLOW,   // Branch, return, exit
    STACK,          // Push/pop literals and values
    VARIABLE,       // Load/store variables
    CALL,           // Function/method calls
    STRING,         // String operations
    ARRAY,          // Array operations
    LOOP,           // For/Next loops
    MEMORY,         // Memory management
    ARITHMETIC,     // Math operations
    LOGICAL,        // Logical operations
    COMPARISON,     // Comparison operations
    CONVERSION,     // Type conversion
    UNKNOWN         // Unknown/unimplemented
};

/// P-Code opcode metadata
struct PCodeOpcodeInfo {
    uint8_t opcode;                     // Primary opcode byte
    uint8_t extOpcode;                  // Extended opcode (if extended)
    std::string_view mnemonic;          // Instruction mnemonic
    std::string_view format;            // Argument format string (from docs)
    PCodeOpcodeCategory category;       // Opcode category
    int stackDelta;                     // Stack depth change (-2 = pop 2, +1 = push 1, etc.)
    bool isExtended;                    // Extended opcode (0xFB-0xFF)
    bool isBranch;                      // Is branch instruction
    bool isConditionalBranch;           // Is conditional branch
    bool isCall;                        // Is function/method call
    bool isReturn;                      // Is return/exit instruction
    
    constexpr PCodeOpcodeInfo(
        uint8_t op, uint8_t extOp, std::string_view mn, std::string_view fmt,
        PCodeOpcodeCategory cat, int stackDelta, bool extended = false,
        bool branch = false, bool condBranch = false,
        bool call = false, bool ret = false)
        : opcode(op), extOpcode(extOp), mnemonic(mn), format(fmt),
          category(cat), stackDelta(stackDelta), isExtended(extended),
          isBranch(branch), isConditionalBranch(condBranch),
          isCall(call), isReturn(ret) {}
};

/// Get opcode info for a standard opcode (0x00-0xFA)
const PCodeOpcodeInfo* getOpcodeInfo(uint8_t opcode);

/// Get opcode info for an extended opcode (0xFB-0xFF + second byte)
const PCodeOpcodeInfo* getExtendedOpcodeInfo(uint8_t primaryOpcode, uint8_t secondaryOpcode);

/// Check if opcode is extended (0xFB-0xFF)
constexpr bool isExtendedOpcode(uint8_t opcode) {
    return opcode >= 0xFB && opcode <= 0xFF;
}

/// Get category name
std::string_view getCategoryName(PCodeOpcodeCategory category);

} // namespace VBDecompiler
