#include "PEFile.h"
#include <QFile>
#include <QDataStream>
#include <algorithm>
#include <cstring>

namespace VBDecompiler {

PEFile::PEFile(const std::filesystem::path& path)
    : path_(path)
{
}

bool PEFile::parse() {
    // Read entire file into memory using Qt
    QFile file(QString::fromStdString(path_.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        setError("Failed to open file: " + path_.string());
        return false;
    }

    QByteArray fileBytes = file.readAll();
    file.close();

    // Convert QByteArray to std::vector<std::byte>
    fileData_.resize(fileBytes.size());
    std::memcpy(fileData_.data(), fileBytes.data(), fileBytes.size());

    // Parse PE structures
    if (!parseDOSHeader()) return false;
    if (!parsePEHeader()) return false;
    if (!parseSections()) return false;
    if (!parseImports()) return false;

    valid_ = true;
    return true;
}

bool PEFile::parseDOSHeader() {
    if (fileData_.size() < sizeof(DOSHeader)) {
        setError("File too small to contain DOS header");
        return false;
    }

    std::memcpy(&dosHeader_, fileData_.data(), sizeof(DOSHeader));

    if (dosHeader_.e_magic != DOS_MAGIC) {
        setError("Invalid DOS signature");
        return false;
    }

    if (dosHeader_.e_lfanew >= fileData_.size()) {
        setError("Invalid PE header offset");
        return false;
    }

    return true;
}

bool PEFile::parsePEHeader() {
    size_t peOffset = dosHeader_.e_lfanew;
    
    if (peOffset + sizeof(PEHeader) > fileData_.size()) {
        setError("File too small to contain PE header");
        return false;
    }

    std::memcpy(&peHeader_, fileData_.data() + peOffset, sizeof(PEHeader));

    if (peHeader_.Signature != PE_SIGNATURE) {
        setError("Invalid PE signature");
        return false;
    }

    if (peHeader_.OptionalHeader.Magic != PE32_MAGIC) {
        setError("Only PE32 (32-bit) executables are supported");
        return false;
    }

    if (peHeader_.FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        setError("Only x86 (i386) executables are supported");
        return false;
    }

    return true;
}

bool PEFile::parseSections() {
    size_t sectionHeaderOffset = dosHeader_.e_lfanew + sizeof(uint32_t) + 
                                  sizeof(COFFHeader) + 
                                  peHeader_.FileHeader.SizeOfOptionalHeader;

    uint16_t numSections = peHeader_.FileHeader.NumberOfSections;
    
    for (uint16_t i = 0; i < numSections; ++i) {
        size_t offset = sectionHeaderOffset + i * sizeof(SectionHeader);
        
        if (offset + sizeof(SectionHeader) > fileData_.size()) {
            setError("Invalid section header offset");
            return false;
        }

        SectionHeader sectionHeader;
        std::memcpy(&sectionHeader, fileData_.data() + offset, sizeof(SectionHeader));

        // Read section data
        std::vector<std::byte> sectionData;
        if (sectionHeader.SizeOfRawData > 0 && sectionHeader.PointerToRawData > 0) {
            size_t dataOffset = sectionHeader.PointerToRawData;
            size_t dataSize = sectionHeader.SizeOfRawData;

            if (dataOffset + dataSize <= fileData_.size()) {
                sectionData.resize(dataSize);
                std::memcpy(sectionData.data(), 
                           fileData_.data() + dataOffset, 
                           dataSize);
            }
        }

        sections_.emplace_back(sectionHeader, std::move(sectionData));
    }

    return true;
}

bool PEFile::parseImports() {
    // Get import directory
    const auto& importDir = peHeader_.OptionalHeader.DataDir[IMAGE_DIRECTORY_ENTRY_IMPORT];
    
    if (importDir.VirtualAddress == 0 || importDir.Size == 0) {
        // No imports (unlikely for VB executables, but not an error)
        return true;
    }

    auto rvaData = getRVAData(importDir.VirtualAddress);
    if (!rvaData) {
        // Imports not found, but continue anyway
        return true;
    }

    const auto& sectionData = rvaData->section.get().getData();
    size_t offset = rvaData->offset;

    // Parse import directory table
    while (offset + sizeof(ImportDirectoryEntry) <= sectionData.size()) {
        ImportDirectoryEntry importEntry;
        std::memcpy(&importEntry, sectionData.data() + offset, sizeof(ImportDirectoryEntry));

        // Check for end of import directory (null entry)
        if (importEntry.NameRVA == 0) {
            break;
        }

        // Get DLL name
        auto dllNameData = readAtRVA(importEntry.NameRVA, 256);
        if (!dllNameData.empty()) {
            std::string dllName;
            for (auto byte : dllNameData) {
                if (byte == std::byte{0}) break;
                dllName += static_cast<char>(byte);
            }

            // TODO: Parse imported function names from ImportLookupTableRVA
            imports_[dllName] = {};
        }

        offset += sizeof(ImportDirectoryEntry);
    }

    return true;
}

const PESection* PEFile::getSectionByName(const std::string& name) const {
    auto it = std::find_if(sections_.begin(), sections_.end(),
        [&name](const PESection& section) {
            return section.getName() == name;
        });
    
    return (it != sections_.end()) ? &(*it) : nullptr;
}

const PESection* PEFile::getSectionByRVA(uint32_t rva) const {
    auto it = std::find_if(sections_.begin(), sections_.end(),
        [rva](const PESection& section) {
            return section.containsRVA(rva);
        });
    
    return (it != sections_.end()) ? &(*it) : nullptr;
}

std::optional<uint32_t> PEFile::rvaToFileOffset(uint32_t rva) const {
    auto rvaData = getRVAData(rva);
    if (!rvaData) {
        return std::nullopt;
    }

    return rvaData->section.get().getRawDataPointer() + static_cast<uint32_t>(rvaData->offset);
}

std::vector<std::byte> PEFile::readAtRVA(uint32_t rva, size_t size) const {
    auto rvaData = getRVAData(rva);
    if (!rvaData) {
        return {};
    }

    const auto& sectionData = rvaData->section.get().getData();
    size_t offset = rvaData->offset;
    
    if (offset + size > sectionData.size()) {
        size = sectionData.size() - offset;
    }

    return std::vector<std::byte>(sectionData.begin() + offset,
                                   sectionData.begin() + offset + size);
}

std::vector<std::string> PEFile::getImportedDLLs() const {
    std::vector<std::string> dlls;
    dlls.reserve(imports_.size());
    
    for (const auto& [dllName, functions] : imports_) {
        dlls.push_back(dllName);
    }
    
    return dlls;
}

std::vector<std::string> PEFile::getImportsFromDLL(const std::string& dllName) const {
    auto it = imports_.find(dllName);
    if (it != imports_.end()) {
        return it->second;
    }
    return {};
}

void PEFile::setError(const std::string& error) {
    lastError_ = error;
    valid_ = false;
}

std::optional<PEFile::RVAData> PEFile::getRVAData(uint32_t rva) const {
    const PESection* section = getSectionByRVA(rva);
    if (!section) {
        return std::nullopt;
    }
    
    int64_t sectionOffset = section->rvaToOffset(rva);
    if (sectionOffset < 0) {
        return std::nullopt;
    }
    
    return RVAData{std::cref(*section), static_cast<size_t>(sectionOffset)};
}

} // namespace VBDecompiler
