#include "core/disasm/x86/X86Disassembler.h"
#include <print>
#include <vector>

using namespace VBDecompiler;

int main() {
    std::println("X86 Disassembler Test");
    std::println("====================\n");

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

    X86Disassembler disasm;
    auto instructions = disasm.disassemble(std::span<const uint8_t>(code.data(), code.size()), 0);

    if (instructions.empty()) {
        std::println(stderr, "Failed to disassemble");
        return 1;
    }

    std::println("Disassembled {} instruction(s):\n", instructions.size());

    for (const auto& instr : instructions) {
        std::print("0x{:08x}  ", instr.getAddress());
        
        // Print bytes
        for (size_t i = 0; i < instr.getLength(); ++i) {
            std::print("{:02x} ", code[instr.getAddress() + i]);
        }
        
        // Padding
        for (size_t i = instr.getLength(); i < 10; ++i) {
            std::print("   ");
        }
        
        std::println("{}", instr.getMnemonic());
    }

    std::println("\nTest PASSED");
    return 0;
}
