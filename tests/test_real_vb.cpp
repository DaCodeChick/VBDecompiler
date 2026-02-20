// Quick test program to analyze real VB binaries
#include "src/core/pe/PEFile.h"
#include "src/core/vb/VBFile.h"
#include "src/core/disasm/pcode/PCodeDisassembler.h"
#include "src/core/ir/PCodeLifter.h"
#include "src/core/decompiler/Decompiler.h"
#include <iostream>
#include <filesystem>
#include <iomanip>

using namespace VBDecompiler;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <vb_file.exe|ocx>\n";
        return 1;
    }
    
    std::filesystem::path filePath(argv[1]);
    std::cout << "Analyzing: " << filePath << "\n";
    std::cout << "File size: " << std::filesystem::file_size(filePath) << " bytes\n\n";
    
    // Step 1: Parse PE
    auto peFile = std::make_unique<PEFile>(filePath);
    if (!peFile->parse()) {
        std::cerr << "PE Parse Error: " << peFile->getLastError() << "\n";
        return 1;
    }
    std::cout << "✓ PE file parsed successfully\n";
    std::cout << "  Image base: 0x" << std::hex << peFile->getImageBase() << std::dec << "\n";
    std::cout << "  Sections: " << peFile->getSections().size() << "\n";
    
    // Step 2: Parse VB structures
    auto vbFile = std::make_unique<VBFile>(std::move(peFile));
    if (!vbFile->parse()) {
        std::cerr << "VB Parse Error: " << vbFile->getLastError() << "\n";
        return 1;
    }
    std::cout << "✓ VB structures parsed successfully\n";
    
    if (!vbFile->isVBFile()) {
        std::cerr << "Error: Not a VB file (no VB5! signature)\n";
        return 1;
    }
    
    std::cout << "  VB Header RVA: 0x" << std::hex << vbFile->getVBHeaderRVA() << std::dec << "\n";
    std::cout << "  Project: " << vbFile->getProjectName() << "\n";
    std::cout << "  P-Code: " << (vbFile->isPCode() ? "Yes" : "No") << "\n";
    std::cout << "  Native: " << (vbFile->isNativeCode() ? "Yes" : "No") << "\n";
    std::cout << "  Objects: " << vbFile->getObjectCount() << "\n\n";
    
    // List all objects
    std::cout << "Objects:\n";
    std::cout << "========\n";
    for (size_t i = 0; i < vbFile->getObjects().size(); ++i) {
        const auto& obj = vbFile->getObjects()[i];
        std::cout << "  [" << i << "] " << obj.name;
        
        if (obj.isForm()) std::cout << " (Form)";
        else if (obj.isModule()) std::cout << " (Module)";
        else if (obj.isClass()) std::cout << " (Class)";
        
        if (obj.info) {
            std::cout << " - " << obj.info->wMethodCount << " methods";
        }
        std::cout << "\n";
        
        // List methods
        if (!obj.methodNames.empty()) {
            for (size_t j = 0; j < obj.methodNames.size(); ++j) {
                std::cout << "      [" << j << "] " << obj.methodNames[j] << "\n";
            }
        }
    }
    
    // If P-Code, try to extract and decompile first method
    if (vbFile->isPCode() && vbFile->getObjectCount() > 0) {
        std::cout << "\n\nAttempting to decompile first method...\n";
        std::cout << "========================================\n";
        
        for (size_t objIdx = 0; objIdx < vbFile->getObjects().size(); ++objIdx) {
            const auto& obj = vbFile->getObjects()[objIdx];
            
            if (obj.methodNames.empty()) continue;
            
            for (size_t methodIdx = 0; methodIdx < obj.methodNames.size(); ++methodIdx) {
                const auto& methodName = obj.methodNames[methodIdx];
                
                std::cout << "\nObject: " << obj.name << ", Method: " << methodName << "\n";
                
                // Extract P-Code
                auto pcodeBytes = vbFile->getPCodeForMethod(
                    static_cast<uint32_t>(objIdx), 
                    static_cast<uint32_t>(methodIdx)
                );
                
                if (pcodeBytes.empty()) {
                    std::cout << "  No P-Code bytes\n";
                    continue;
                }
                
                std::cout << "  P-Code size: " << pcodeBytes.size() << " bytes\n";
                
                // Hex dump first 32 bytes
                std::cout << "  First bytes: ";
                for (size_t i = 0; i < std::min(size_t(32), pcodeBytes.size()); ++i) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << static_cast<int>(pcodeBytes[i]) << " ";
                }
                std::cout << std::dec << "\n";
                
                // Try to disassemble
                PCodeDisassembler disasm;
                std::span<const uint8_t> pcodeSpan(pcodeBytes.data(), pcodeBytes.size());
                auto instructions = disasm.disassembleProcedure(pcodeSpan, 0, 0);
                
                if (instructions.empty()) {
                    std::cout << "  Disassembly failed: " << disasm.getLastError() << "\n";
                    continue;
                }
                
                std::cout << "  Disassembled: " << instructions.size() << " instructions\n";
                
                // Show first few instructions
                std::cout << "  First instructions:\n";
                for (size_t i = 0; i < std::min(size_t(5), instructions.size()); ++i) {
                    std::cout << "    " << instructions[i].getMnemonic() << "\n";
                }
                
                // Try to lift to IR
                PCodeLifter lifter;
                auto irFunc = lifter.lift(instructions, methodName, 0);
                
                if (!irFunc) {
                    std::cout << "  IR lift failed: " << lifter.getLastError() << "\n";
                    continue;
                }
                
                std::cout << "  IR lifted: " << irFunc->getBasicBlocks().size() << " basic blocks\n";
                
                // Try to decompile
                Decompiler decompiler;
                std::string vbCode = decompiler.decompile(*irFunc);
                
                std::cout << "\n  Decompiled VB6 code:\n";
                std::cout << "  -------------------\n";
                std::cout << vbCode << "\n";
                
                // Only decompile first method for now
                return 0;
            }
        }
    }
    
    return 0;
}

