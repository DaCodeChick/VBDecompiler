#include "core/disasm/x86/X86Disassembler.h"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace VBDecompiler;

int main() {
    std::cout << "X86 Disassembler Test\n";
    std::cout << "====================\n\n";

    // Simple x86 code: MOV EAX, 42; RET
    std::vector<uint8_t> code = {
        0xB8, 0x2A, 0x00, 0x00, 0x00,  // MOV EAX, 42
        0xC3                           // RET
    };

    std::cout << "Disassembling bytes: ";
    for (auto byte : code) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << "\n\n";

    X86Disassembler disasm;
    auto instructions = disasm.disassemble(std::span<const uint8_t>(code.data(), code.size()), 0);

    if (instructions.empty()) {
        std::cerr << "Failed to disassemble\n";
        return 1;
    }

    std::cout << "Disassembled " << instructions.size() << " instruction(s):\n\n";

    for (const auto& instr : instructions) {
        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') 
                  << instr.getAddress() << "  ";
        
        // Print bytes
        for (size_t i = 0; i < instr.getLength(); ++i) {
            std::cout << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(code[instr.getAddress() + i]) << " ";
        }
        
        // Padding
        for (size_t i = instr.getLength(); i < 10; ++i) {
            std::cout << "   ";
        }
        
        std::cout << std::dec << instr.getMnemonic() << "\n";
    }

    std::cout << "\nTest PASSED\n";
    return 0;
}
