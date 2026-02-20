#include "VBFile.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace VBDecompiler {

VBFile::VBFile(std::shared_ptr<PEFile> peFile)
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

    // VB addresses are relative to image base
    uint32_t imageBase = peFile_->getImageBase();
    uint32_t projectInfoRVA = vbHeader_.lpProjectInfo - imageBase;

    auto infoOpt = readStructAtRVA<VBProjectInfo>(projectInfoRVA);
    if (!infoOpt) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), 
                     "Failed to read project info at RVA 0x%X (VA 0x%X, imageBase 0x%X)", 
                     projectInfoRVA, vbHeader_.lpProjectInfo, imageBase);
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

    // VB addresses are relative to image base
    uint32_t imageBase = peFile_->getImageBase();
    uint32_t objectTableRVA = projectInfo_->lpObjectTable - imageBase;

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

    // Try to read project name from various locations
    uint32_t imageBase = peFile_->getImageBase();

    // Try bSZProjectName from VB header
    if (vbHeader_.bSZProjectName != 0) {
        uint32_t nameRVA = vbHeader_.bSZProjectName - imageBase;
        std::string name = readStringAtRVA(nameRVA);
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
