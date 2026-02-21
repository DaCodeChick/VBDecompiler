#include "core/disasm/x86/X86Disassembler.h"
#include <format>
#include <vector>
#include <cstdio>

using namespace VBDecompiler;

int main() {
    std::puts("X86 Disassembler Test");
    std::puts("====================\n");

    // Simple x86 code: MOV EAX, 42; RET
    std::vector<uint8_t> code = {
        0xB8, 0x2A, 0x00, 0x00, 0x00,  // MOV EAX, 42
        0xC3                           // RET
    };

    std::fputs("Disassembling bytes: ", stdout);
    for (auto byte : code) {
        std::fputs(std::format("{:02x} ", byte).c_str(), stdout);
    }
    std::puts("\n");

    X86Disassembler disasm;
    auto instructions = disasm.disassemble(std::span<const uint8_t>(code.data(), code.size()), 0);

    if (instructions.empty()) {
        std::fputs("Failed to disassemble\n", stderr);
        return 1;
    }

    std::puts(std::format("Disassembled {} instruction(s):\n", instructions.size()).c_str());

    for (const auto& instr : instructions) {
        std::fputs(std::format("0x{:08x}  ", instr.getAddress()).c_str(), stdout);
        
        // Print bytes
        for (size_t i = 0; i < instr.getLength(); ++i) {
            std::fputs(std::format("{:02x} ", code[instr.getAddress() + i]).c_str(), stdout);
        }
        
        // Padding
        for (size_t i = instr.getLength(); i < 10; ++i) {
            std::fputs("   ", stdout);
        }
        
        std::puts(instr.getMnemonic().c_str());
    }

    std::puts("\nTest PASSED");
    return 0;
}
