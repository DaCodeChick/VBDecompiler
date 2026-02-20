#ifndef PEHEADER_H
#define PEHEADER_H

#include <cstdint>
#include <array>

namespace VBDecompiler {

// DOS Header (MZ header)
#pragma pack(push, 1)
struct DOSHeader {
    uint16_t e_magic;      // Magic number "MZ" (0x5A4D)
    uint16_t e_cblp;       // Bytes on last page of file
    uint16_t e_cp;         // Pages in file
    uint16_t e_crlc;       // Relocations
    uint16_t e_cparhdr;    // Size of header in paragraphs
    uint16_t e_minalloc;   // Minimum extra paragraphs needed
    uint16_t e_maxalloc;   // Maximum extra paragraphs needed
    uint16_t e_ss;         // Initial (relative) SS value
    uint16_t e_sp;         // Initial SP value
    uint16_t e_csum;       // Checksum
    uint16_t e_ip;         // Initial IP value
    uint16_t e_cs;         // Initial (relative) CS value
    uint16_t e_lfarlc;     // File address of relocation table
    uint16_t e_ovno;       // Overlay number
    uint16_t e_res[4];     // Reserved words
    uint16_t e_oemid;      // OEM identifier
    uint16_t e_oeminfo;    // OEM information
    uint16_t e_res2[10];   // Reserved words
    uint32_t e_lfanew;     // File address of new exe header (PE header)
};

// COFF File Header
struct COFFHeader {
    uint16_t Machine;              // Machine type
    uint16_t NumberOfSections;     // Number of sections
    uint32_t TimeDateStamp;        // Time/date stamp
    uint32_t PointerToSymbolTable; // Pointer to symbol table
    uint32_t NumberOfSymbols;      // Number of symbols
    uint16_t SizeOfOptionalHeader; // Size of optional header
    uint16_t Characteristics;      // Characteristics
};

// Data Directory
struct DataDirectory {
    uint32_t VirtualAddress;
    uint32_t Size;
};

// Optional Header (PE32)
struct OptionalHeader32 {
    uint16_t Magic;                    // 0x10b for PE32
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    
    // NT additional fields
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    DataDirectory DataDir[16];
};

// PE Header (NT Headers)
struct PEHeader {
    uint32_t Signature;           // PE signature "PE\0\0" (0x00004550)
    COFFHeader FileHeader;
    OptionalHeader32 OptionalHeader;
};

// Section Header
struct SectionHeader {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

// Import Directory Entry
struct ImportDirectoryEntry {
    uint32_t ImportLookupTableRVA;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t NameRVA;
    uint32_t ImportAddressTableRVA;
};

#pragma pack(pop)

// Constants
constexpr uint16_t DOS_MAGIC = 0x5A4D;      // "MZ"
constexpr uint32_t PE_SIGNATURE = 0x00004550; // "PE\0\0"
constexpr uint16_t PE32_MAGIC = 0x10B;
constexpr uint16_t PE64_MAGIC = 0x20B;

// Machine types
constexpr uint16_t IMAGE_FILE_MACHINE_I386 = 0x014C;  // x86
constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664; // x64

// Characteristics
constexpr uint16_t IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002;
constexpr uint16_t IMAGE_FILE_DLL = 0x2000;

// Section characteristics
constexpr uint32_t IMAGE_SCN_CNT_CODE = 0x00000020;
constexpr uint32_t IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040;
constexpr uint32_t IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080;
constexpr uint32_t IMAGE_SCN_MEM_EXECUTE = 0x20000000;
constexpr uint32_t IMAGE_SCN_MEM_READ = 0x40000000;
constexpr uint32_t IMAGE_SCN_MEM_WRITE = 0x80000000;

// Data directory indices
constexpr uint32_t IMAGE_DIRECTORY_ENTRY_EXPORT = 0;
constexpr uint32_t IMAGE_DIRECTORY_ENTRY_IMPORT = 1;
constexpr uint32_t IMAGE_DIRECTORY_ENTRY_RESOURCE = 2;

} // namespace VBDecompiler

#endif // PEHEADER_H
