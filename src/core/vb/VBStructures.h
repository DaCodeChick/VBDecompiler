#ifndef VBSTRUCTURES_H
#define VBSTRUCTURES_H

#include <cstdint>

namespace VBDecompiler {

// VB5/6 binary structures
// Based on research from Semi-VB-Decompiler and reverse engineering

#pragma pack(push, 1)

/**
 * @brief VB5/6 Header structure
 * 
 * The main VB header, identified by "VB5!" signature.
 * Located at varying offsets in the binary, typically in .data or .rdata section.
 */
struct VBHeader {
    char szVbMagic[4];              // 0x00 - "VB5!" identifier
    uint16_t wRuntimeBuild;         // 0x04 - Runtime build number
    char szLanguageDLL[14];         // 0x06 - Language DLL name (0x2A = default/null)
    char szSecLanguageDLL[14];      // 0x14 - Backup Language DLL
    uint16_t wRuntimeDLLVersion;    // 0x22 - Runtime DLL version
    uint32_t dwLCID;                // 0x24 - Language ID
    uint32_t dwSecLCID;             // 0x28 - Backup Language ID
    uint32_t lpSubMain;             // 0x2C - Address to Sub Main() (0 = load form)
    uint32_t lpProjectInfo;         // 0x30 - Address to Project Info
    uint32_t fMDLIntObjs;           // 0x34 - MDL Internal Objects flag
    uint32_t fMDLIntObjs2;          // 0x38 - MDL Internal Objects flag 2
    uint32_t dwThreadFlags;         // 0x3C - Thread flags
    uint32_t dwThreadCount;         // 0x40 - Thread count
    uint16_t wFormCount;            // 0x44 - Number of forms
    uint16_t wExternalCount;        // 0x46 - External components count
    uint32_t dwThunkCount;          // 0x48 - Thunk count
    uint32_t lpGuiTable;            // 0x4C - Address to GUI Table
    uint32_t lpExternalComponentTable; // 0x50 - External component table address
    uint32_t lpComRegisterData;     // 0x54 - COM registration data address
    uint32_t bSZProjectDescription; // 0x58 - Offset to project description
    uint32_t bSZProjectExeName;     // 0x5C - Offset to project EXE name
    uint32_t bSZProjectHelpFile;    // 0x60 - Offset to help file
    uint32_t bSZProjectName;        // 0x64 - Offset to project name
};  // Size: 0x68 (104 bytes)

/**
 * @brief Project Information structure
 * 
 * Contains project-level information and pointers to object tables.
 */
struct VBProjectInfo {
    uint32_t dwVersion;             // 0x00 - Signature/version
    uint32_t lpObjectTable;         // 0x04 - Address to Object Table
    uint32_t dwNull;                // 0x08 - Null/Reserved
    uint32_t lpCodeStart;           // 0x0C - Start of code address
    uint32_t lpCodeEnd;             // 0x10 - End of code address
    uint32_t dwDataSize;            // 0x14 - Data size
    uint32_t lpThreadSpace;         // 0x18 - Thread space
    uint32_t lpVbaSeh;              // 0x1C - VBA exception handler
    uint32_t lpNativeCode;          // 0x20 - Native code address
    char szPath1[260];              // 0x24 - Path/Name (260 bytes)
    char szPath2[260];              // 0x128 - Secondary path (260 bytes)
    uint32_t lpExternalTable;       // 0x22C - External table address
    uint32_t dwExternalCount;       // 0x230 - External count
};  // Size: 0x234 (564 bytes)

/**
 * @brief Object Table structure header
 * 
 * Header for the object table, followed by array of object descriptors.
 */
struct VBObjectTableHeader {
    uint32_t lpHeapLink;            // 0x00 - Heap link
    uint32_t lpExecProj;            // 0x04 - Execution project
    uint32_t lpProjectInfo2;        // 0x08 - Project info 2
    uint16_t wReserved;             // 0x0C - Reserved
    uint16_t wTotalObjects;         // 0x0E - Total number of objects
    uint16_t wCompiledObjects;      // 0x10 - Compiled objects
    uint16_t wObjectsInUse;         // 0x12 - Objects in use
    uint32_t lpObjectArray;         // 0x14 - Pointer to object array
    uint32_t fIdeFlag;              // 0x18 - IDE data flag
    uint32_t fIdeFlag2;             // 0x1C - IDE data flag 2
    uint32_t lpIdeData;             // 0x20 - IDE data pointer
    uint32_t lpIdeData2;            // 0x24 - IDE data pointer 2
    uint32_t lpszProjectName;       // 0x28 - Project name pointer
    uint32_t dwLcid;                // 0x2C - LCID
    uint32_t dwLcid2;               // 0x30 - LCID 2
    uint32_t lpIdeData3;            // 0x34 - IDE data pointer 3
    uint32_t dwIdentifier;          // 0x38 - Template version
};  // Size: 0x3C (60 bytes)

/**
 * @brief Public Object Descriptor
 * 
 * Describes a VB object (form, class, module, etc.)
 */
struct VBPublicObjectDescriptor {
    uint32_t lpObjectInfo;          // 0x00 - Object info pointer
    uint32_t dwReserved;            // 0x04 - Reserved
    uint32_t lpPublicBytes;         // 0x08 - Public bytes pointer
    uint32_t lpStaticBytes;         // 0x0C - Static bytes pointer
    uint32_t lpModulePublic;        // 0x10 - Module public pointer
    uint32_t lpModuleStatic;        // 0x14 - Module static pointer
    uint32_t lpszObjectName;        // 0x18 - Object name pointer
    uint32_t dwMethodCount;         // 0x1C - Method count
    uint32_t lpMethodNamesArray;    // 0x20 - Method names array pointer
    uint32_t bStaticVars;           // 0x24 - Static vars offset
    uint32_t fObjectType;           // 0x28 - Object type
    uint32_t dwNull;                // 0x2C - Null
};  // Size: 0x30 (48 bytes)

/**
 * @brief Object Info structure
 * 
 * Contains detailed information about an object.
 */
struct VBObjectInfo {
    uint16_t wRefCount;             // 0x00 - Reference count
    uint16_t wObjectIndex;          // 0x02 - Object index
    uint32_t lpObjectTable;         // 0x04 - Object table pointer
    uint32_t lpIdeData;             // 0x08 - IDE data
    uint32_t lpPrivateObject;       // 0x0C - Private object pointer
    uint32_t dwReserved;            // 0x10 - Reserved
    uint32_t dwNull;                // 0x14 - Null
    uint32_t lpObject;              // 0x18 - Object pointer
    uint32_t lpProjectData;         // 0x1C - Project data
    uint16_t wMethodCount;          // 0x20 - Method count
    uint16_t wMethodCount2;         // 0x22 - Method count 2
    uint32_t lpMethods;             // 0x24 - Methods pointer
    uint16_t wConstants;            // 0x28 - Constants
    uint16_t wMaxConstants;         // 0x2A - Max constants
    uint32_t lpIdeData2;            // 0x2C - IDE data 2
    uint32_t lpIdeData3;            // 0x30 - IDE data 3
    uint32_t lpConstants;           // 0x34 - Constants pointer
};  // Size: 0x38 (56 bytes)

/**
 * @brief Optional Object Information
 * 
 * Additional information for forms and controls (present when ObjectType & 0x80).
 */
struct VBOptionalObjectInfo {
    uint32_t dwDesignerFlag;        // 0x00 - Designer flag (2 = designer)
    uint32_t lpObjectCLSID;         // 0x04 - Object CLSID pointer
    uint32_t dwNull1;               // 0x08 - Null
    uint32_t lpGuidObjectGUI;       // 0x0C - GUI GUID pointer
    uint32_t dwDefaultIIDCount;     // 0x10 - Default IID count
    uint32_t lpEventsIIDTable;      // 0x14 - Events IID table pointer
    uint32_t dwEventsIIDCount;      // 0x18 - Events IID count
    uint32_t lpDefaultIIDTable;     // 0x1C - Default IID table pointer
    uint32_t dwControlCount;        // 0x20 - Control count
    uint32_t lpControlArray;        // 0x24 - Control array pointer
    uint16_t wEventCount;           // 0x28 - Number of events
    uint16_t wPCodeCount;           // 0x2A - P-Code count
    uint16_t wInitializeEvent;      // 0x2C - Initialize event offset
    uint16_t wTerminateEvent;       // 0x2E - Terminate event offset
    uint32_t lpEventLinkArray;      // 0x30 - Event link array pointer
    uint32_t lpBasicClassObject;    // 0x34 - Basic class object pointer
    uint32_t dwNull2;               // 0x38 - Null
    uint32_t dwFlags;               // 0x3C - Flags
};  // Size: 0x40 (64 bytes)

/**
 * @brief Procedure Descriptor Information (P-Code)
 * 
 * Describes a procedure/method in P-Code format.
 */
struct VBProcDescInfo {
    uint32_t lpTable;               // 0x00 - Table pointer
    uint16_t wReserved1;            // 0x04 - Reserved
    uint16_t wFrameSize;            // 0x06 - Stack frame size
    uint16_t wProcSize;             // 0x08 - Procedure size in bytes
    uint16_t wReserved2;            // 0x0A - Reserved
    uint16_t wReserved3;            // 0x0C - Reserved
    uint16_t wReserved4;            // 0x0E - Reserved
    uint16_t wReserved5;            // 0x10 - Reserved
    uint16_t wReserved6;            // 0x12 - Reserved
    uint16_t wReserved7;            // 0x14 - Reserved
    uint16_t wReserved8;            // 0x16 - Reserved
    uint16_t wReserved9;            // 0x18 - Reserved
    uint16_t wReserved10;           // 0x1A - Reserved
    uint16_t wFlags;                // 0x1C - Flags
};  // Size: 0x1E (30 bytes)

/**
 * @brief Control Descriptor
 * 
 * Describes a control on a form.
 */
struct VBControlInfo {
    uint16_t wDlgProcIndex;         // 0x00 - Dialog procedure index
    uint16_t wReserved1;            // 0x02 - Reserved
    uint32_t lpControlName;         // 0x04 - Control name pointer
    uint32_t lpIdeData;             // 0x08 - IDE data pointer
    uint32_t lpIndex;               // 0x0C - Index pointer
    uint32_t lpTypeInfo;            // 0x10 - Type info pointer
    uint32_t lpGuidControl;         // 0x14 - Control GUID pointer
    uint32_t dwPosition;            // 0x18 - Position/size data
    uint32_t dwFlags;               // 0x1C - Flags
    uint16_t wControlIndex;         // 0x20 - Control index
    uint16_t wReserved2;            // 0x22 - Reserved
    uint32_t dwExtent;              // 0x24 - Extent data
    uint32_t lpNext;                // 0x28 - Next control pointer
};  // Size: 0x2C (44 bytes)

/**
 * @brief Method/Procedure Name Entry
 * 
 * Entry in the method names array for an object.
 */
struct VBMethodName {
    uint32_t lpMethodName;          // 0x00 - Method name pointer
    uint32_t dwFlags;               // 0x04 - Flags (visibility, type, etc.)
};  // Size: 0x08 (8 bytes)

#pragma pack(pop)

// Constants
constexpr char VB5_MAGIC[4] = {'V', 'B', '5', '!'};

// Thread flags
constexpr uint32_t THREAD_FLAG_APARTMENT = 0x01;      // Apartment model
constexpr uint32_t THREAD_FLAG_REQUIRE_LICENSE = 0x02; // Require license (OCX)
constexpr uint32_t THREAD_FLAG_UNATTENDED = 0x04;     // No GUI
constexpr uint32_t THREAD_FLAG_SINGLETHREADED = 0x08; // Single-threaded
constexpr uint32_t THREAD_FLAG_RETAINED = 0x10;       // Retained in memory

// Object types
constexpr uint32_t OBJECT_TYPE_DESIGNER = 0x00;       // Designer
constexpr uint32_t OBJECT_TYPE_CLASS_MODULE = 0x02;   // Class module
constexpr uint32_t OBJECT_TYPE_FORM = 0x10;           // Form
constexpr uint32_t OBJECT_TYPE_MODULE = 0x11;         // Standard module

} // namespace VBDecompiler

#endif // VBSTRUCTURES_H
