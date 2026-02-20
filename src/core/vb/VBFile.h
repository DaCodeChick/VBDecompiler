#ifndef VBFILE_H
#define VBFILE_H

#include "VBStructures.h"
#include "../pe/PEFile.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstring>

namespace VBDecompiler {

/**
 * @brief VB5/6 binary file parser
 * 
 * Detects and parses Visual Basic 5/6 specific structures within PE executables.
 */
class VBFile {
public:
    /**
     * @brief Construct a VBFile from a PE file
     * @param peFile Parsed PE file
     */
    explicit VBFile(std::shared_ptr<PEFile> peFile);

    /**
     * @brief Parse the VB structures
     * @return true if parsing succeeded, false otherwise
     */
    bool parse();

    /**
     * @brief Check if this is a valid VB file
     */
    [[nodiscard]] bool isValid() const { return valid_; }

    /**
     * @brief Check if this is a VB file (has VB5! signature)
     */
    [[nodiscard]] bool isVBFile() const { return hasVBHeader_; }

    /**
     * @brief Check if compiled to P-Code
     * 
     * Determines if the VB executable uses P-Code (bytecode) or native x86.
     * Generally: if lpNativeCode == 0, it's P-Code.
     */
    [[nodiscard]] bool isPCode() const {
        return valid_ && hasVBHeader_ && !isNativeCode_;
    }

    /**
     * @brief Check if compiled to native code
     */
    [[nodiscard]] bool isNativeCode() const {
        return valid_ && hasVBHeader_ && isNativeCode_;
    }

    /**
     * @brief Get the VB header
     */
    [[nodiscard]] const VBHeader& getVBHeader() const { return vbHeader_; }

    /**
     * @brief Get the project info
     */
    [[nodiscard]] const std::optional<VBProjectInfo>& getProjectInfo() const {
        return projectInfo_;
    }

    /**
     * @brief Get the object table header
     */
    [[nodiscard]] const std::optional<VBObjectTableHeader>& getObjectTableHeader() const {
        return objectTableHeader_;
    }

    /**
     * @brief Get the number of objects
     */
    [[nodiscard]] uint16_t getObjectCount() const {
        return objectTableHeader_ ? objectTableHeader_->wTotalObjects : 0;
    }

    /**
     * @brief Get project name if available
     */
    [[nodiscard]] std::string getProjectName() const;

    /**
     * @brief Get the underlying PE file
     */
    [[nodiscard]] std::shared_ptr<PEFile> getPEFile() const { return peFile_; }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& getLastError() const { return lastError_; }

    /**
     * @brief Get the RVA where VB header was found
     */
    [[nodiscard]] uint32_t getVBHeaderRVA() const { return vbHeaderRVA_; }

private:
    bool findVBHeader();
    bool parseVBHeader();
    bool parseProjectInfo();
    bool parseObjectTable();

    /**
     * @brief Read a structure at an RVA
     * @tparam T Structure type
     * @param rva Relative Virtual Address
     * @return Optional containing the structure if successful
     */
    template<typename T>
    std::optional<T> readStructAtRVA(uint32_t rva) {
        auto data = peFile_->readAtRVA(rva, sizeof(T));
        if (data.size() < sizeof(T)) {
            return std::nullopt;
        }

        T result;
        std::memcpy(&result, data.data(), sizeof(T));
        return result;
    }

    /**
     * @brief Read a null-terminated string at an RVA
     * @param rva Relative Virtual Address
     * @param maxLength Maximum length to read
     * @return The string, or empty string if failed
     */
    std::string readStringAtRVA(uint32_t rva, size_t maxLength = 256) const;

    void setError(const std::string& error);

    std::shared_ptr<PEFile> peFile_;
    bool valid_ = false;
    bool hasVBHeader_ = false;
    bool isNativeCode_ = false;
    std::string lastError_;

    uint32_t vbHeaderRVA_ = 0;
    VBHeader vbHeader_{};
    std::optional<VBProjectInfo> projectInfo_;
    std::optional<VBObjectTableHeader> objectTableHeader_;
};

} // namespace VBDecompiler

#endif // VBFILE_H
