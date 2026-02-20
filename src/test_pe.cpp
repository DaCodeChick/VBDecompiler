#include "core/pe/PEFile.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace VBDecompiler;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pe_file>" << std::endl;
        return 1;
    }

    std::string filePath = argv[1];
    std::cout << "Parsing PE file: " << filePath << std::endl << std::endl;

    PEFile peFile(filePath);
    
    if (!peFile.parse()) {
        std::cerr << "Error: " << peFile.getLastError() << std::endl;
        return 1;
    }

    std::cout << "✓ PE file parsed successfully!" << std::endl << std::endl;

    // Display DOS header info
    const auto& dosHeader = peFile.getDOSHeader();
    std::cout << "DOS Header:" << std::endl;
    std::cout << "  Magic: 0x" << std::hex << dosHeader.e_magic << std::dec << " (MZ)" << std::endl;
    std::cout << "  PE offset: 0x" << std::hex << dosHeader.e_lfanew << std::dec << std::endl << std::endl;

    // Display PE header info
    const auto& peHeader = peFile.getPEHeader();
    std::cout << "PE Header:" << std::endl;
    std::cout << "  Signature: 0x" << std::hex << peHeader.Signature << std::dec << " (PE)" << std::endl;
    std::cout << "  Machine: 0x" << std::hex << peHeader.FileHeader.Machine << std::dec;
    if (peHeader.FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
        std::cout << " (i386)";
    }
    std::cout << std::endl;
    std::cout << "  Number of sections: " << peHeader.FileHeader.NumberOfSections << std::endl;
    std::cout << "  Is DLL: " << (peFile.isDLL() ? "Yes" : "No") << std::endl;
    std::cout << "  Is Executable: " << (peFile.isExecutable() ? "Yes" : "No") << std::endl;
    std::cout << "  Image base: 0x" << std::hex << peFile.getImageBase() << std::dec << std::endl;
    std::cout << "  Entry point RVA: 0x" << std::hex << peFile.getEntryPointRVA() << std::dec << std::endl << std::endl;

    // Display sections
    std::cout << "Sections:" << std::endl;
    std::cout << "  " << std::left << std::setw(10) << "Name" 
              << std::setw(12) << "VirtAddr" 
              << std::setw(12) << "VirtSize"
              << std::setw(12) << "RawSize"
              << "Flags" << std::endl;
    std::cout << "  " << std::string(60, '-') << std::endl;

    for (const auto& section : peFile.getSections()) {
        std::cout << "  " << std::left << std::setw(10) << section.getName()
                  << "0x" << std::hex << std::setw(10) << section.getVirtualAddress()
                  << "0x" << std::setw(10) << section.getVirtualSize()
                  << "0x" << std::setw(10) << section.getRawDataSize()
                  << std::dec;

        // Display flags
        if (section.isExecutable()) std::cout << "X";
        if (section.isReadable()) std::cout << "R";
        if (section.isWritable()) std::cout << "W";
        if (section.containsCode()) std::cout << " CODE";
        if (section.containsInitializedData()) std::cout << " DATA";

        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Display imports
    auto importedDLLs = peFile.getImportedDLLs();
    std::cout << "Imported DLLs (" << importedDLLs.size() << "):" << std::endl;
    for (const auto& dll : importedDLLs) {
        std::cout << "  - " << dll << std::endl;
    }
    std::cout << std::endl;

    // Check for VB runtime
    bool hasVBRuntime = false;
    for (const auto& dll : importedDLLs) {
        std::string dllLower = dll;
        std::transform(dllLower.begin(), dllLower.end(), dllLower.begin(), ::tolower);
        if (dllLower.find("msvbvm") != std::string::npos) {
            hasVBRuntime = true;
            std::cout << "✓ VB Runtime detected: " << dll << std::endl;
            std::cout << "  This appears to be a Visual Basic executable!" << std::endl;
            break;
        }
    }

    if (!hasVBRuntime) {
        std::cout << "⚠ No VB runtime detected" << std::endl;
        std::cout << "  This may not be a Visual Basic executable" << std::endl;
    }

    return 0;
}
