#ifndef PESECTION_H
#define PESECTION_H

#include "PEHeader.h"
#include <string>
#include <vector>
#include <cstddef>

namespace VBDecompiler {

/**
 * @brief Represents a PE section
 */
class PESection {
public:
    PESection(const SectionHeader& header, std::vector<std::byte> data)
        : header_(header), data_(std::move(data)) {}

    /**
     * @brief Get the section name (max 8 chars)
     */
    [[nodiscard]] std::string getName() const {
        std::string name;
        for (size_t i = 0; i < 8 && header_.Name[i] != 0; ++i) {
            name += static_cast<char>(header_.Name[i]);
        }
        return name;
    }

    /**
     * @brief Get the virtual address (RVA)
     */
    [[nodiscard]] uint32_t getVirtualAddress() const {
        return header_.VirtualAddress;
    }

    /**
     * @brief Get the virtual size
     */
    [[nodiscard]] uint32_t getVirtualSize() const {
        return header_.VirtualSize;
    }

    /**
     * @brief Get the raw data size
     */
    [[nodiscard]] uint32_t getRawDataSize() const {
        return header_.SizeOfRawData;
    }

    /**
     * @brief Get the raw data pointer (file offset)
     */
    [[nodiscard]] uint32_t getRawDataPointer() const {
        return header_.PointerToRawData;
    }

    /**
     * @brief Get section characteristics
     */
    [[nodiscard]] uint32_t getCharacteristics() const {
        return header_.Characteristics;
    }

    /**
     * @brief Check if section is executable
     */
    [[nodiscard]] bool isExecutable() const {
        return (header_.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    }

    /**
     * @brief Check if section is readable
     */
    [[nodiscard]] bool isReadable() const {
        return (header_.Characteristics & IMAGE_SCN_MEM_READ) != 0;
    }

    /**
     * @brief Check if section is writable
     */
    [[nodiscard]] bool isWritable() const {
        return (header_.Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
    }

    /**
     * @brief Check if section contains code
     */
    [[nodiscard]] bool containsCode() const {
        return (header_.Characteristics & IMAGE_SCN_CNT_CODE) != 0;
    }

    /**
     * @brief Check if section contains initialized data
     */
    [[nodiscard]] bool containsInitializedData() const {
        return (header_.Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0;
    }

    /**
     * @brief Get the section data
     */
    [[nodiscard]] const std::vector<std::byte>& getData() const {
        return data_;
    }

    /**
     * @brief Get the section header
     */
    [[nodiscard]] const SectionHeader& getHeader() const {
        return header_;
    }

    /**
     * @brief Check if an RVA falls within this section
     */
    [[nodiscard]] bool containsRVA(uint32_t rva) const {
        return rva >= header_.VirtualAddress && 
               rva < header_.VirtualAddress + header_.VirtualSize;
    }

    /**
     * @brief Convert RVA to offset within section data
     * @return Offset within section data, or -1 if RVA not in this section
     */
    [[nodiscard]] int64_t rvaToOffset(uint32_t rva) const {
        if (!containsRVA(rva)) {
            return -1;
        }
        return rva - header_.VirtualAddress;
    }

private:
    SectionHeader header_;
    std::vector<std::byte> data_;
};

} // namespace VBDecompiler

#endif // PESECTION_H
