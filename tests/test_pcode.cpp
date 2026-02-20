#include "core/pe/PEFile.h"
#include "core/vb/VBFile.h"
#include "core/disasm/pcode/PCodeDisassembler.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <map>

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

    std::cout << "Disassembling P-Code from VB file: " << filePath << std::endl;
    std::cout << "Starting at RVA: 0x" << std::hex << std::uppercase << rva << std::dec << std::endl << std::endl;

    // Parse PE file
    auto peFile = std::make_shared<PEFile>(filePath);
    if (!peFile->parse()) {
        std::cerr << "Error parsing PE: " << peFile->getLastError() << std::endl;
        return 1;
    }

    // Parse VB structures
    VBFile vbFile(peFile);
    if (!vbFile.parse()) {
        std::cerr << "Error parsing VB structures: " << vbFile.getLastError() << std::endl;
        return 1;
    }

    // Check if P-Code
    if (vbFile.isNativeCode()) {
        std::cerr << "Warning: This is native x86 code, not P-Code. Use test_x86 instead." << std::endl;
    }

    // Read P-Code at RVA
    constexpr size_t MAX_PCODE_SIZE = 512;  // Disassemble up to 512 bytes
    auto pcodeBytes = peFile->readAtRVA(rva, MAX_PCODE_SIZE);
    
    if (pcodeBytes.empty()) {
        std::cerr << "Error: Could not read P-Code at RVA 0x" << std::hex << rva << std::dec << std::endl;
        return 1;
    }

    // Convert std::byte vector to uint8_t for disassembler
    std::vector<uint8_t> pcode;
    pcode.reserve(pcodeBytes.size());
    for (auto byte : pcodeBytes) {
        pcode.push_back(static_cast<uint8_t>(byte));
    }

    // Disassemble P-Code
    PCodeDisassembler disasm;
    uint32_t address = peFile->getImageBase() + rva;
    
    auto instructions = disasm.disassemble(std::span{pcode}, 0, address, 50);  // Limit to 50 instructions
    
    if (instructions.empty()) {
        std::cerr << "Error: Failed to disassemble any P-Code instructions" << std::endl;
        std::cerr << "Last error: " << disasm.getLastError() << std::endl;
        return 1;
    }

    // Display disassembly
    std::cout << "Address        Bytes             Mnemonic                 Operands" << std::endl;
    std::cout << "=============  ================  =======================  ============================" << std::endl;
    
    for (const auto& instr : instructions) {
        // Address column
        std::cout << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                  << instr.getAddress() << "   ";
        
        // Bytes column (up to 6 bytes, padded)
        std::cout << std::left << std::setw(16) << std::setfill(' ') << instr.bytesToHex() << "  ";
        
        // Mnemonic column
        std::cout << std::left << std::setw(23) << std::setfill(' ') << instr.getMnemonic() << "  ";
        
        // Operands
        std::cout << instr.toString() << std::endl;
        
        // Stop at procedure exit
        if (instr.getMnemonic() == "ExitProc" || instr.getMnemonic() == "ExitProcHresult") {
            break;
        }
    }
    
    std::cout << std::dec << std::endl;
    std::cout << "Disassembled " << instructions.size() << " P-Code instructions" << std::endl;
    
    // Show category summary
    std::map<PCodeOpcodeCategory, int> categoryCounts;
    for (const auto& instr : instructions) {
        categoryCounts[instr.getCategory()]++;
    }
    
    std::cout << "\nInstruction categories:" << std::endl;
    for (const auto& [category, count] : categoryCounts) {
        std::cout << "  " << getCategoryName(category) << ": " << count << std::endl;
    }

    return 0;
}
