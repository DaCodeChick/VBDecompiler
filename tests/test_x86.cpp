#include "core/pe/PEFile.h"
#include "core/vb/VBFile.h"
#include "core/disasm/x86/X86Disassembler.h"
#include <iostream>
#include <iomanip>
#include <memory>

using namespace VBDecompiler;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <vb_file> <rva_hex>" << std::endl;
        std::cerr << "Example: " << argv[0] << " program.exe 0x1000" << std::endl;
        return 1;
    }

    std::string filePath = argv[1];
    std::string rvaStr = argv[2];
    
    // Parse RVA
    uint32_t rva;
    try {
        rva = std::stoul(rvaStr, nullptr, 16);
    } catch (...) {
        std::cerr << "Invalid RVA: " << rvaStr << std::endl;
        return 1;
    }

    std::cout << "Disassembling VB file: " << filePath << std::endl;
    std::cout << "Starting at RVA: 0x" << std::hex << std::uppercase << rva << std::dec << std::endl << std::endl;

    // Parse PE file
    auto peFile = std::make_unique<PEFile>(filePath);
    if (!peFile->parse()) {
        std::cerr << "Error parsing PE: " << peFile->getLastError() << std::endl;
        return 1;
    }

    // Parse VB structures (transfer ownership)
    VBFile vbFile(std::move(peFile));
    if (!vbFile.parse()) {
        std::cerr << "Error parsing VB structures: " << vbFile.getLastError() << std::endl;
        return 1;
    }

    // Check if native code
    if (!vbFile.isNativeCode()) {
        std::cerr << "Warning: This is P-Code, not native x86. Disassembly may not work correctly." << std::endl;
    }

    // Read code at RVA
    constexpr size_t MAX_CODE_SIZE = 512;  // Disassemble up to 512 bytes
    auto codeBytes = peFile->readAtRVA(rva, MAX_CODE_SIZE);
    
    if (codeBytes.empty()) {
        std::cerr << "Error: Could not read code at RVA 0x" << std::hex << rva << std::dec << std::endl;
        return 1;
    }

    // Convert std::byte vector to uint8_t for disassembler
    std::vector<uint8_t> code;
    code.reserve(codeBytes.size());
    for (auto byte : codeBytes) {
        code.push_back(static_cast<uint8_t>(byte));
    }

    // Disassemble
    X86Disassembler disasm;
    uint32_t address = peFile->getImageBase() + rva;
    
    auto instructions = disasm.disassemble(std::span{code}, address, 30);  // Limit to 30 instructions
    
    if (instructions.empty()) {
        std::cerr << "Error: Failed to disassemble any instructions" << std::endl;
        std::cerr << "Last error: " << disasm.getLastError() << std::endl;
        return 1;
    }

    // Display disassembly
    std::cout << "Address        Bytes                    Mnemonic" << std::endl;
    std::cout << "=============  =======================  ========================================" << std::endl;
    
    for (const auto& instr : instructions) {
        // Address column
        std::cout << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                  << instr.getAddress() << "   ";
        
        // Bytes column (up to 8 bytes, padded)
        std::string bytesStr = instr.getBytesString();
        std::cout << std::left << std::setw(23) << std::setfill(' ') << bytesStr << "  ";
        
        // Mnemonic and operands
        std::cout << instr.toString() << std::endl;
        
        // Stop at RET
        if (instr.isReturn()) {
            break;
        }
    }
    
    std::cout << std::dec << std::endl;
    std::cout << "Disassembled " << instructions.size() << " instructions" << std::endl;

    return 0;
}
