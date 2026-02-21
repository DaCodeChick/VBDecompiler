// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * Test for Rust-based x86 disassembler via FFI
 */

#include "vbdecompiler_ffi.h"
#include <print>
#include <vector>
#include <cstdint>

int main() {
    std::println("X86 Disassembler Test (Rust Backend)");
    std::println("=====================================\n");

    // Simple x86 code: MOV EAX, 42; RET
    std::vector<uint8_t> code = {
        0xB8, 0x2A, 0x00, 0x00, 0x00,  // MOV EAX, 42
        0xC3                           // RET
    };

    std::print("Disassembling bytes: ");
    for (auto byte : code) {
        std::print("{:02x} ", byte);
    }
    std::println("\n");

    // Create Rust disassembler via FFI
    auto* disasm = x86_disassembler_new();
    if (!disasm) {
        std::println(stderr, "Failed to create disassembler");
        return 1;
    }

    // Disassemble
    X86InstructionResult* results = nullptr;
    size_t count = 0;
    int ret = x86_disassemble(disasm, code.data(), code.size(), 0, &results, &count);

    if (ret < 0) {
        std::println(stderr, "Failed to disassemble");
        x86_disassembler_free(disasm);
        return 1;
    }

    std::println("Disassembled {} instruction(s):\n", count);

    // Print instructions
    for (size_t i = 0; i < count; ++i) {
        const auto& instr = results[i];
        
        std::print("0x{:08x}  ", instr.address);
        
        // Print bytes
        for (size_t j = 0; j < instr.bytes_count; ++j) {
            std::print("{:02x} ", instr.bytes[j]);
        }
        
        // Padding
        for (size_t j = instr.bytes_count; j < 10; ++j) {
            std::print("   ");
        }
        
        std::println("{}", instr.text ? instr.text : "(null)");
    }

    // Cleanup
    x86_disassembler_free_results(results, count);
    x86_disassembler_free(disasm);

    std::println("\nTest PASSED");
    std::println("Rust x86 disassembler is working!");
    return 0;
}
