#include "VBFile.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace VBDecompiler {

VBFile::VBFile(std::unique_ptr<PEFile> peFile)
    : peFile_(std::move(peFile))
{
}

bool VBFile::parse() {
    if (!peFile_ || !peFile_->isValid()) {
        setError("Invalid PE file");
        return false;
    }

    // Find VB5! header signature
    if (!findVBHeader()) {
        setError("VB5! header signature not found");
        return false;
    }

    // Parse VB header
    if (!parseVBHeader()) {
        return false;
    }

    // Parse project info
    if (!parseProjectInfo()) {
        return false;
    }

    // Parse object table
    if (!parseObjectTable()) {
        return false;
    }
    
    // Parse all objects (forms, modules, classes)
    if (!parseObjects()) {
        return false;
    }

    valid_ = true;
    return true;
}

bool VBFile::findVBHeader() {
    // Search for "VB5!" signature in all sections
    // Typically found in .data or .rdata sections

    for (const auto& section : peFile_->getSections()) {
        const auto& data = section.getData();
        
        // Search for VB5! signature
        for (size_t i = 0; i + 4 <= data.size(); ++i) {
            if (data[i] == std::byte{'V'} &&
                data[i+1] == std::byte{'B'} &&
                data[i+2] == std::byte{'5'} &&
                data[i+3] == std::byte{'!'}) {
                
                // Found VB5! signature
                // Calculate RVA
                vbHeaderRVA_ = section.getVirtualAddress() + static_cast<uint32_t>(i);
                hasVBHeader_ = true;
                return true;
            }
        }
    }

    return false;
}

bool VBFile::parseObjects() {
    if (!objectTableHeader_ || objectTableHeader_->wTotalObjects == 0) {
        // No objects to parse (shouldn't happen in valid VB files)
        return true;
    }
    
    uint32_t objectArrayRVA = vaToRVA(objectTableHeader_->lpObjectArray);
    
    // Parse each object descriptor
    for (uint16_t i = 0; i < objectTableHeader_->wTotalObjects; ++i) {
        uint32_t objRVA = objectArrayRVA + (i * sizeof(VBPublicObjectDescriptor));
        
        auto objDescOpt = readStructAtRVA<VBPublicObjectDescriptor>(objRVA);
        if (!objDescOpt) {
            // Skip invalid objects
            continue;
        }
        
        VBObject obj;
        obj.descriptor = *objDescOpt;
        obj.objectIndex = i;
        
        // Parse object name
        if (obj.descriptor.lpszObjectName != 0) {
            obj.name = readStringAtRVA(vaToRVA(obj.descriptor.lpszObjectName));
        }
        
        // Parse object info
        if (obj.descriptor.lpObjectInfo != 0) {
            parseObjectInfo(obj, vaToRVA(obj.descriptor.lpObjectInfo));
        }
        
        // Parse optional info (for forms with controls)
        if (obj.hasOptionalInfo() && obj.info) {
            // OptionalInfo follows ObjectInfo structure
            uint32_t optInfoRVA = vaToRVA(obj.descriptor.lpObjectInfo) + sizeof(VBObjectInfo);
            parseOptionalObjectInfo(obj, optInfoRVA);
        }
        
        // Parse method names
        parseMethodNames(obj);
        
        objects_.push_back(std::move(obj));
    }
    
    return true;
}

bool VBFile::parseObjectInfo(VBObject& obj, uint32_t rva) {
    auto infoOpt = readStructAtRVA<VBObjectInfo>(rva);
    if (!infoOpt) {
        return false;
    }
    
    obj.info = *infoOpt;
    return true;
}

bool VBFile::parseOptionalObjectInfo(VBObject& obj, uint32_t rva) {
    auto optInfoOpt = readStructAtRVA<VBOptionalObjectInfo>(rva);
    if (!optInfoOpt) {
        return false;
    }
    
    obj.optionalInfo = *optInfoOpt;
    return true;
}

bool VBFile::parseMethodNames(VBObject& obj) {
    if (obj.descriptor.dwMethodCount == 0 || obj.descriptor.lpMethodNamesArray == 0) {
        return true;  // No methods
    }
    
    uint32_t namesArrayRVA = vaToRVA(obj.descriptor.lpMethodNamesArray);
    
    // Read array of method name pointers
    for (uint32_t i = 0; i < obj.descriptor.dwMethodCount; ++i) {
        uint32_t entryRVA = namesArrayRVA + (i * sizeof(VBMethodName));
        
        auto nameEntry = readStructAtRVA<VBMethodName>(entryRVA);
        if (!nameEntry || nameEntry->lpMethodName == 0) {
            obj.methodNames.push_back("<unknown>");
            continue;
        }
        
        std::string methodName = readStringAtRVA(vaToRVA(nameEntry->lpMethodName));
        obj.methodNames.push_back(methodName.empty() ? "<unnamed>" : methodName);
    }
    
    return true;
}

const VBObject* VBFile::getObjectByName(const std::string& name) const {
    for (const auto& obj : objects_) {
        if (obj.name == name) {
            return &obj;
        }
    }
    return nullptr;
}

std::vector<uint8_t> VBFile::getPCodeForMethod(uint32_t objectIndex, uint32_t methodIndex) const {
    // Verify valid state
    if (!valid_ || !isPCode()) {
        return {};
    }
    
    // Verify object index
    if (objectIndex >= objects_.size()) {
        return {};
    }
    
    const auto& obj = objects_[objectIndex];
    
    // Verify object has info and methods pointer
    if (!obj.info || obj.info->lpMethods == 0) {
        return {};
    }
    
    // Verify method index
    if (methodIndex >= obj.info->wMethodCount) {
        return {};
    }
    
    // Read method table entry
    // The lpMethods pointer points to an array of procedure descriptors
    uint32_t methodTableRVA = vaToRVA(obj.info->lpMethods);
    uint32_t procDescRVA = methodTableRVA + (methodIndex * sizeof(VBProcDescInfo));
    
    auto procDescOpt = readStructAtRVA<VBProcDescInfo>(procDescRVA);
    if (!procDescOpt) {
        return {};
    }
    
    // The P-Code immediately follows the VBProcDescInfo structure
    uint32_t pcodeRVA = procDescRVA + sizeof(VBProcDescInfo);
    uint16_t pcodeSize = procDescOpt->wProcSize;
    
    if (pcodeSize == 0) {
        return {};
    }
    
    // Read P-Code bytes
    auto pcodeBytes = peFile_->readAtRVA(pcodeRVA, pcodeSize);
    
    // Convert std::vector<std::byte> to std::vector<uint8_t>
    std::vector<uint8_t> result;
    result.reserve(pcodeBytes.size());
    for (auto byte : pcodeBytes) {
        result.push_back(static_cast<uint8_t>(byte));
    }
    
    return result;
}

std::vector<std::vector<uint8_t>> VBFile::getAllPCodeForObject(uint32_t objectIndex) const {
    std::vector<std::vector<uint8_t>> result;
    
    // Verify valid state
    if (!valid_ || !isPCode()) {
        return result;
    }
    
    // Verify object index
    if (objectIndex >= objects_.size()) {
        return result;
    }
    
    const auto& obj = objects_[objectIndex];
    
    // Verify object has info
    if (!obj.info) {
        return result;
    }
    
    // Extract P-Code for each method
    result.reserve(obj.info->wMethodCount);
    for (uint16_t i = 0; i < obj.info->wMethodCount; ++i) {
        result.push_back(getPCodeForMethod(objectIndex, i));
    }
    
    return result;
}

bool VBFile::parseVBHeader() {
    auto headerOpt = readStructAtRVA<VBHeader>(vbHeaderRVA_);
    if (!headerOpt) {
        setError("Failed to read VB header at RVA 0x" + std::to_string(vbHeaderRVA_));
        return false;
    }

    vbHeader_ = *headerOpt;

    // Validate signature
    if (std::memcmp(vbHeader_.szVbMagic, VB5_MAGIC, 4) != 0) {
        setError("Invalid VB header signature");
        return false;
    }

    return true;
}

bool VBFile::parseProjectInfo() {
    if (vbHeader_.lpProjectInfo == 0) {
        setError("No project info pointer in VB header");
        return false;
    }

    uint32_t projectInfoRVA = vaToRVA(vbHeader_.lpProjectInfo);
    
    // Sanity check: RVA should be within reasonable bounds
    if (projectInfoRVA > peFile_->getSections().back().getVirtualAddress() + 
                         peFile_->getSections().back().getVirtualSize()) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), 
                     "Project info RVA 0x%X is out of bounds (VA 0x%X, imageBase 0x%X)", 
                     projectInfoRVA, vbHeader_.lpProjectInfo, peFile_->getImageBase());
        setError(buf);
        return false;
    }

    auto infoOpt = readStructAtRVA<VBProjectInfo>(projectInfoRVA);
    if (!infoOpt) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), 
                     "Failed to read project info at RVA 0x%X (VA 0x%X, imageBase 0x%X)", 
                     projectInfoRVA, vbHeader_.lpProjectInfo, peFile_->getImageBase());
        setError(buf);
        return false;
    }

    projectInfo_ = *infoOpt;

    // Determine if P-Code or native
    isNativeCode_ = (projectInfo_->lpNativeCode != 0);

    return true;
}

bool VBFile::parseObjectTable() {
    if (!projectInfo_ || projectInfo_->lpObjectTable == 0) {
        setError("No object table pointer in project info");
        return false;
    }

    uint32_t objectTableRVA = vaToRVA(projectInfo_->lpObjectTable);

    auto tableOpt = readStructAtRVA<VBObjectTableHeader>(objectTableRVA);
    if (!tableOpt) {
        setError("Failed to read object table header");
        return false;
    }

    objectTableHeader_ = *tableOpt;

    return true;
}

std::string VBFile::getProjectName() const {
    if (!projectInfo_) {
        return "";
    }

    // Try bSZProjectName from VB header
    if (vbHeader_.bSZProjectName != 0) {
        std::string name = readStringAtRVA(vaToRVA(vbHeader_.bSZProjectName));
        if (!name.empty()) {
            return name;
        }
    }

    // Try path from project info
    if (projectInfo_->szPath1[0] != '\0') {
        return std::string(projectInfo_->szPath1);
    }

    return "";
}

std::string VBFile::readStringAtRVA(uint32_t rva, size_t maxLength) const {
    auto data = peFile_->readAtRVA(rva, maxLength);
    if (data.empty()) {
        return "";
    }

    std::string result;
    for (auto byte : data) {
        if (byte == std::byte{0}) {
            break;
        }
        result += static_cast<char>(byte);
    }

    return result;
}

void VBFile::setError(const std::string& error) {
    lastError_ = error;
    valid_ = false;
}

} // namespace VBDecompiler
