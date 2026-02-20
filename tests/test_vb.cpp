#include "core/pe/PEFile.h"
#include "core/vb/VBFile.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <algorithm>

using namespace VBDecompiler;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <vb_file>" << std::endl;
        return 1;
    }

    std::string filePath = argv[1];
    std::cout << "Analyzing VB file: " << filePath << std::endl << std::endl;

    // Parse PE file
    auto peFile = std::make_unique<PEFile>(filePath);
    if (!peFile->parse()) {
        std::cerr << "Error parsing PE: " << peFile->getLastError() << std::endl;
        return 1;
    }

    std::cout << "✓ PE file parsed successfully" << std::endl;

    // Check for VB runtime
    bool hasVBRuntime = false;
    for (const auto& dll : peFile->getImportedDLLs()) {
        std::string dllLower = dll;
        std::transform(dllLower.begin(), dllLower.end(), dllLower.begin(), ::tolower);
        if (dllLower.find("msvbvm") != std::string::npos) {
            hasVBRuntime = true;
            std::cout << "✓ VB Runtime detected: " << dll << std::endl;
            break;
        }
    }

    if (!hasVBRuntime) {
        std::cout << "⚠ No VB runtime detected" << std::endl;
    }

    // Parse VB structures (transfer ownership to VBFile)
    VBFile vbFile(std::move(peFile));
    if (!vbFile.parse()) {
        std::cerr << "Error parsing VB structures: " << vbFile.getLastError() << std::endl;
        return 1;
    }

    std::cout << "✓ VB structures parsed successfully!" << std::endl << std::endl;

    // Display VB header info
    const auto& vbHeader = vbFile.getVBHeader();
    
    std::cout << "VB Header:" << std::endl;
    std::cout << "  Signature: " << std::string(vbHeader.szVbMagic, 4) << std::endl;
    std::cout << "  Runtime Build: " << vbHeader.wRuntimeBuild << std::endl;
    std::cout << "  Runtime DLL Version: " << vbHeader.wRuntimeDLLVersion << std::endl;
    std::cout << "  LCID: 0x" << std::hex << vbHeader.dwLCID << std::dec << std::endl;
    std::cout << "  Form Count: " << vbHeader.wFormCount << std::endl;
    std::cout << "  External Count: " << vbHeader.wExternalCount << std::endl;
    std::cout << "  Thread Flags: 0x" << std::hex << vbHeader.dwThreadFlags << std::dec;
    
    // Decode thread flags
    if (vbHeader.dwThreadFlags & THREAD_FLAG_APARTMENT) std::cout << " APARTMENT";
    if (vbHeader.dwThreadFlags & THREAD_FLAG_SINGLETHREADED) std::cout << " SINGLETHREADED";
    if (vbHeader.dwThreadFlags & THREAD_FLAG_UNATTENDED) std::cout << " UNATTENDED";
    std::cout << std::endl;

    std::cout << "  Sub Main: 0x" << std::hex << vbHeader.lpSubMain << std::dec;
    if (vbHeader.lpSubMain == 0) {
        std::cout << " (Load form)";
    }
    std::cout << std::endl;
    std::cout << "  Project Info: 0x" << std::hex << vbHeader.lpProjectInfo << std::dec << std::endl;
    std::cout << std::endl;

    // Display project info
    if (vbFile.getProjectInfo()) {
        const auto& projInfo = *vbFile.getProjectInfo();
        
        std::cout << "Project Info:" << std::endl;
        std::cout << "  Version: 0x" << std::hex << projInfo.dwVersion << std::dec << std::endl;
        std::cout << "  Object Table: 0x" << std::hex << projInfo.lpObjectTable << std::dec << std::endl;
        std::cout << "  Code Start: 0x" << std::hex << projInfo.lpCodeStart << std::dec << std::endl;
        std::cout << "  Code End: 0x" << std::hex << projInfo.lpCodeEnd << std::dec << std::endl;
        std::cout << "  Data Size: " << projInfo.dwDataSize << " bytes" << std::endl;
        std::cout << "  Native Code: 0x" << std::hex << projInfo.lpNativeCode << std::dec;
        
        if (vbFile.isPCode()) {
            std::cout << " (P-Code)";
        } else if (vbFile.isNativeCode()) {
            std::cout << " (Native x86)";
        }
        std::cout << std::endl;

        // Try to display project name
        std::string projectName = vbFile.getProjectName();
        if (!projectName.empty()) {
            std::cout << "  Project Name: " << projectName << std::endl;
        }
        
        std::cout << std::endl;
    }

    // Display object table info
    if (vbFile.getObjectTableHeader()) {
        const auto& objTable = *vbFile.getObjectTableHeader();
        
        std::cout << "Object Table:" << std::endl;
        std::cout << "  Total Objects: " << objTable.wTotalObjects << std::endl;
        std::cout << "  Compiled Objects: " << objTable.wCompiledObjects << std::endl;
        std::cout << "  Objects In Use: " << objTable.wObjectsInUse << std::endl;
        std::cout << "  Object Array: 0x" << std::hex << objTable.lpObjectArray << std::dec << std::endl;
        std::cout << std::endl;
    }

    // Display parsed objects
    const auto& objects = vbFile.getObjects();
    if (!objects.empty()) {
        std::cout << "Parsed Objects (" << objects.size() << "):" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (size_t i = 0; i < objects.size(); ++i) {
            const auto& obj = objects[i];
            
            std::cout << "[" << i << "] " << (obj.name.empty() ? "<unnamed>" : obj.name);
            
            // Display object type
            if (obj.isForm()) {
                std::cout << " (Form)";
            } else if (obj.isModule()) {
                std::cout << " (Module)";
            } else if (obj.isClass()) {
                std::cout << " (Class)";
            }
            std::cout << std::endl;
            
            // Display descriptor info
            const auto& desc = obj.descriptor;
            std::cout << "  Object Info: 0x" << std::hex << desc.lpObjectInfo << std::dec;
            if (desc.dwReserved & 0x02) std::cout << " DESIGNER";
            if (desc.dwReserved & 0x08) std::cout << " CLASS_MODULE";
            std::cout << std::endl;
            
            std::cout << "  Object Type: 0x" << std::hex << desc.fObjectType << std::dec << std::endl;
            
            // Display method count
            if (desc.dwMethodCount > 0) {
                std::cout << "  Methods (" << desc.dwMethodCount << "):" << std::endl;
                
                for (size_t j = 0; j < obj.methodNames.size() && j < desc.dwMethodCount; ++j) {
                    std::cout << "    [" << j << "] " << obj.methodNames[j] << std::endl;
                }
                
                if (obj.methodNames.size() < desc.dwMethodCount) {
                    std::cout << "    ... (" << (desc.dwMethodCount - obj.methodNames.size()) 
                             << " methods not parsed)" << std::endl;
                }
            } else {
                std::cout << "  Methods: none" << std::endl;
            }
            
            // Display optional info (forms with controls)
            if (obj.hasOptionalInfo() && obj.optionalInfo) {
                std::cout << "  Optional Info:" << std::endl;
                std::cout << "    Control Count: " << obj.optionalInfo->dwControlCount << std::endl;
                std::cout << "    Event Count: " << obj.optionalInfo->wEventCount << std::endl;
                std::cout << "    Control Array: 0x" << std::hex << obj.optionalInfo->lpControlArray << std::dec << std::endl;
                std::cout << "    Event Link Array: 0x" << std::hex << obj.optionalInfo->lpEventLinkArray << std::dec << std::endl;
            }
            
            std::cout << std::endl;
        }
    }

    // Summary
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  VB Version: " << (hasVBRuntime ? "VB5/VB6" : "Unknown") << std::endl;
    std::cout << "  Compilation Mode: ";
    if (vbFile.isPCode()) {
        std::cout << "P-Code (VB Bytecode)" << std::endl;
    } else if (vbFile.isNativeCode()) {
        std::cout << "Native Code (x86)" << std::endl;
    } else {
        std::cout << "Unknown" << std::endl;
    }
    std::cout << "  Objects: " << vbFile.getObjectCount() << std::endl;
    std::cout << "  Forms: " << std::count_if(objects.begin(), objects.end(), 
                                               [](const VBObject& o) { return o.isForm(); }) << std::endl;
    std::cout << "  Modules: " << std::count_if(objects.begin(), objects.end(), 
                                                 [](const VBObject& o) { return o.isModule(); }) << std::endl;
    std::cout << "  Classes: " << std::count_if(objects.begin(), objects.end(), 
                                                 [](const VBObject& o) { return o.isClass(); }) << std::endl;

    return 0;
}
