#ifndef PEFILE_H
#define PEFILE_H

#include "PEHeader.h"
#include "PESection.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace VBDecompiler {

/**
 * @brief PE (Portable Executable) file parser
 * 
 * Parses and provides access to PE file structures including
 * headers, sections, imports, and exports.
 */
class PEFile {
public:
    /**
     * @brief Construct a PE file from a filesystem path
     * @param path Path to the PE file
     */
    explicit PEFile(const std::filesystem::path& path);

    /**
     * @brief Parse the PE file
     * @return true if parsing succeeded, false otherwise
     */
    bool parse();

    /**
     * @brief Check if the file is a valid PE file
     */
    [[nodiscard]] bool isValid() const { return valid_; }

    /**
     * @brief Check if the file is a DLL
     */
    [[nodiscard]] bool isDLL() const {
        return valid_ && (peHeader_.FileHeader.Characteristics & IMAGE_FILE_DLL) != 0;
    }

    /**
     * @brief Check if the file is executable
     */
    [[nodiscard]] bool isExecutable() const {
        return valid_ && (peHeader_.FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) != 0;
    }

    /**
     * @brief Get the DOS header
     */
    [[nodiscard]] const DOSHeader& getDOSHeader() const { return dosHeader_; }

    /**
     * @brief Get the PE header
     */
    [[nodiscard]] const PEHeader& getPEHeader() const { return peHeader_; }

    /**
     * @brief Get all sections
     */
    [[nodiscard]] const std::vector<PESection>& getSections() const { return sections_; }

    /**
     * @brief Get a section by name
     * @param name Section name (e.g., ".text", ".data")
     * @return Pointer to section if found, nullptr otherwise
     */
    [[nodiscard]] const PESection* getSectionByName(const std::string& name) const;

    /**
     * @brief Get a section containing the given RVA
     * @param rva Relative Virtual Address
     * @return Pointer to section if found, nullptr otherwise
     */
    [[nodiscard]] const PESection* getSectionByRVA(uint32_t rva) const;

    /**
     * @brief Convert RVA to file offset
     * @param rva Relative Virtual Address
     * @return File offset if valid, nullopt otherwise
     */
    [[nodiscard]] std::optional<uint32_t> rvaToFileOffset(uint32_t rva) const;

    /**
     * @brief Read data at a given RVA
     * @param rva Relative Virtual Address
     * @param size Number of bytes to read
     * @return Vector of bytes if successful, empty vector otherwise
     */
    [[nodiscard]] std::vector<std::byte> readAtRVA(uint32_t rva, size_t size) const;

    /**
     * @brief Get imported DLL names
     * @return Vector of imported DLL names
     */
    [[nodiscard]] std::vector<std::string> getImportedDLLs() const;

    /**
     * @brief Get imported function names from a specific DLL
     * @param dllName Name of the DLL
     * @return Vector of imported function names
     */
    [[nodiscard]] std::vector<std::string> getImportsFromDLL(const std::string& dllName) const;

    /**
     * @brief Get the file path
     */
    [[nodiscard]] const std::filesystem::path& getFilePath() const { return path_; }

    /**
     * @brief Get the raw file data
     */
    [[nodiscard]] const std::vector<std::byte>& getRawData() const { return fileData_; }

    /**
     * @brief Get the image base address
     */
    [[nodiscard]] uint32_t getImageBase() const {
        return valid_ ? peHeader_.OptionalHeader.ImageBase : 0;
    }

    /**
     * @brief Get the entry point RVA
     */
    [[nodiscard]] uint32_t getEntryPointRVA() const {
        return valid_ ? peHeader_.OptionalHeader.AddressOfEntryPoint : 0;
    }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& getLastError() const { return lastError_; }

private:
    /// Result type for RVA lookup operations
    struct RVAData {
        std::reference_wrapper<const PESection> section;
        size_t offset;
    };

    bool parseDOSHeader();
    bool parsePEHeader();
    bool parseSections();
    bool parseImports();

    void setError(const std::string& error);
    
    /**
     * @brief Helper to get section and offset for an RVA
     * @param rva Relative Virtual Address
     * @return Optional containing section pointer and offset if RVA is valid
     */
    [[nodiscard]] std::optional<RVAData> getRVAData(uint32_t rva) const;

    std::filesystem::path path_;
    std::vector<std::byte> fileData_;
    bool valid_ = false;
    std::string lastError_;

    DOSHeader dosHeader_{};
    PEHeader peHeader_{};
    std::vector<PESection> sections_;
    
    // Import information
    std::unordered_map<std::string, std::vector<std::string>> imports_;
};

} // namespace VBDecompiler

#endif // PEFILE_H
