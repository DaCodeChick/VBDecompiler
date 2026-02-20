# Visual Basic 5/6 Binary Structures Reference

This document contains the complete structure definitions extracted from the Semi-VB-Decompiler project, which will serve as the foundation for our VB decompiler implementation.

## Table of Contents
1. [VB Header Structures](#vb-header-structures)
2. [Object & Table Structures](#object--table-structures)
3. [P-Code Structures](#p-code-structures)
4. [COM/Component Structures](#comcomponent-structures)
5. [Control & Event Structures](#control--event-structures)
6. [Import/Export Structures](#importexport-structures)

---

## VB Header Structures

### VBHeader
Main VB5/6 executable header structure.

```c
struct VBHeader {
    char signature[4];              // 0x00 - "VB5!" identifier
    uint16_t RuntimeBuild;          // 0x04 - Runtime build number
    char LanguageDLL[14];           // 0x06 - Language DLL name (0x2A = default/null)
    char BackupLanguageDLL[14];     // 0x14 - Backup Language DLL
    uint16_t RuntimeDLLVersion;     // 0x22 - Runtime DLL version
    uint32_t LanguageID;            // 0x24 - Language ID
    uint32_t BackupLanguageID;      // 0x28 - Backup Language ID
    uint32_t aSubMain;              // 0x2C - Address to Sub Main() (0 = load form)
    uint32_t aProjectInfo;          // 0x30 - Address to Project Info
    uint32_t fMDLIntObjs;           // 0x34 - MDL Internal Objects flag
    uint32_t fMDLIntObjs2;          // 0x38 - MDL Internal Objects flag 2
    uint32_t ThreadFlags;           // 0x3C - Thread flags
    uint32_t ThreadCount;           // 0x40 - Thread count
    uint16_t FormCount;             // 0x44 - Number of forms
    uint16_t ExternalComponentCount;// 0x46 - External components count
    uint32_t ThunkCount;            // 0x4A - Thunk count
    uint32_t aGuiTable;             // 0x4E - Address to GUI Table
    uint32_t aExternalComponentTable; // 0x52 - External component table address
    uint32_t aComRegisterData;      // 0x56 - COM registration data address
    uint32_t oProjectExename;       // 0x5A - Offset to project EXE name
    uint32_t oProjectTitle;         // 0x5E - Offset to project title
    uint32_t oHelpFile;             // 0x62 - Offset to help file
    uint32_t oProjectName;          // 0x66 - Offset to project name
};  // Size: 0x6A (106 bytes)
```

**Thread Flags:**
- `0x01` - ApartmentModel: Multi-threading using apartment model
- `0x02` - RequireLicense: License validation (OCX only)
- `0x04` - Unattended: No GUI elements
- `0x08` - SingleThreaded: Single-threaded image
- `0x10` - Retained: Keep file in memory (Unattended only)

**MDL Internal Object Flags (First Flag):**
| Ctrl ID | Value      | Object Name   |
|---------|------------|---------------|
| 0x00    | 0x00000001 | PictureBox    |
| 0x01    | 0x00000002 | Label         |
| 0x02    | 0x00000004 | TextBox       |
| 0x03    | 0x00000008 | Frame         |
| 0x04    | 0x00000010 | CommandButton |
| 0x05    | 0x00000020 | CheckBox      |
| 0x06    | 0x00000040 | OptionButton  |
| 0x07    | 0x00000080 | ComboBox      |
| 0x08    | 0x00000100 | ListBox       |
| 0x09    | 0x00000200 | HScrollBar    |
| 0x0A    | 0x00000400 | VScrollBar    |
| 0x0B    | 0x00000800 | Timer         |
| 0x0C    | 0x00001000 | Print         |
| 0x0D    | 0x00002000 | Form          |
| 0x0E    | 0x00004000 | Screen        |
| 0x0F    | 0x00008000 | Clipboard     |
| 0x10    | 0x00010000 | Drive         |
| 0x11    | 0x00020000 | Dir           |
| 0x12    | 0x00040000 | FileListBox   |
| 0x13    | 0x00080000 | Menu          |
| 0x14    | 0x00100000 | MDIForm       |
| 0x15    | 0x00200000 | App           |
| 0x16    | 0x00400000 | Shape         |
| 0x17    | 0x00800000 | Line          |
| 0x18    | 0x01000000 | Image         |

---

## Object & Table Structures

### tProjectInfo
Project information structure.

```c
struct tProjectInfo {
    uint32_t signature;             // 0x00 - Signature
    uint32_t aObjectTable;          // 0x04 - Address to Object Table
    uint32_t Null1;                 // 0x08 - Null/Reserved
    uint32_t aStartOfCode;          // 0x0C - Start of code address
    uint32_t aEndOfCode;            // 0x10 - End of code address
    uint32_t Flag1;                 // 0x14 - Flags
    uint32_t ThreadSpace;           // 0x18 - Thread space
    uint32_t aVBAExceptionhandler;  // 0x1C - VBA Exception handler address
    uint32_t aNativeCode;           // 0x20 - Native code address
    uint16_t oProjectLocation;      // 0x24 - Project location offset
    uint16_t Flag2;                 // 0x26 - Flags
    uint16_t Flag3;                 // 0x28 - Flags
    uint8_t OriginalPathName[520];  // 0x2A - Original path (MAX_PATH * 2)
    uint8_t NullSpacer;             // 0x233 - Null spacer
    uint32_t aExternalTable;        // 0x234 - External table address
    uint32_t ExternalCount;         // 0x238 - External count
};  // Size: 0x23C (572 bytes)
```

### tObjectTable
Main object table structure.

```c
struct tObjectTable {
    uint32_t lNull1;                // 0x00
    uint32_t aExecProj;             // 0x04 - Pointer to memory structure
    uint32_t aProjectInfo2;         // 0x08 - Pointer to Project Info 2
    uint32_t Const1;                // 0x0C
    uint32_t Null2;                 // 0x10
    uint32_t lpProjectObject;       // 0x14
    uint32_t Flag1;                 // 0x18
    uint32_t Flag2;                 // 0x1C
    uint32_t Flag3;                 // 0x20
    uint32_t Flag4;                 // 0x24
    uint16_t fCompileType;          // 0x28 - Compilation type flag
    uint16_t ObjectCount1;          // 0x2A - Object count
    uint16_t iCompiledObjects;      // 0x2C - Number of compiled objects
    uint16_t iObjectsInUse;         // 0x2E - Objects in use
    uint32_t aObject;               // 0x30 - Address to first object
    uint32_t Null3;                 // 0x34
    uint32_t Null4;                 // 0x38
    uint32_t Null5;                 // 0x3C
    uint32_t aProjectName;          // 0x40 - Project name (NTS)
    uint32_t LangID1;               // 0x44 - Language ID
    uint32_t LangID2;               // 0x48 - Language ID 2
    uint32_t Null6;                 // 0x4C
    uint32_t Const3;                // 0x50
};  // Size: 0x54 (84 bytes)
```

### tObject
Object descriptor structure.

```c
struct tObject {
    uint32_t aObjectInfo;           // 0x00 - Address to ObjectInfo
    uint32_t Const1;                // 0x04 - Constant
    uint32_t aPublicBytes;          // 0x08 - Pointer to public variable sizes
    uint32_t aStaticBytes;          // 0x0C - Pointer to static variables struct
    uint32_t aModulePublic;         // 0x10 - Memory pointer to public variables
    uint32_t aModuleStatic;         // 0x14 - Pointer to static variables
    uint32_t aObjectName;           // 0x18 - Object name (NTS)
    uint32_t ProcCount;             // 0x1C - Procedure count (events, funcs, subs)
    uint32_t aProcNamesArray;       // 0x20 - Procedure names array (when non-zero)
    uint32_t oStaticVars;           // 0x24 - Offset to static vars from aModuleStatic
    uint32_t ObjectType;            // 0x28 - Object type
    uint32_t Null3;                 // 0x2C
};  // Size: 0x30 (48 bytes)
```

**tObject.ObjectType Values:**
```
Form:        0x18083, 0x180A3, 0x180C3
Module:      0x18001, 0x18021
Class:       0x118003, 0x138003, 0x18023, 0x18803, 0x118803
UserControl: 0x1DA003, 0x1DA023, 0x1DA803
PropertyPage: 0x158003

Bit flags (from MSB to LSB):
- HasPublicInterface
- HasPublicEvents
- IsCreatable/Visible
- UserControl (1)
- OCX/DLL (1)
- Form (1)
- VB5 (1)
- HasOptInfo (1)
- Module (0)
```

### tObjectInfo
Extended object information.

```c
struct tObjectInfo {
    uint16_t Flag1;                 // 0x00 - Flags
    uint16_t ObjectIndex;           // 0x02 - Object index
    uint32_t aObjectTable;          // 0x04 - Address to object table
    uint32_t Null1;                 // 0x08
    uint32_t aSmallRecord;          // 0x0C - Small record address (-1 for modules)
    uint32_t Const1;                // 0x10
    uint32_t Null2;                 // 0x14
    uint32_t aObject;               // 0x18 - Address to object
    uint32_t RunTimeLoaded;         // 0x1C - Runtime loaded flag
    uint32_t NumberOfProcs;         // 0x20 - Number of procedures
    uint32_t aProcTable;            // 0x24 - Procedure table address
    uint16_t iConstantsCount;       // 0x28 - Number of constants
    uint16_t iMaxConstants;         // 0x2A - Maximum constants to allocate
    uint32_t Flag5;                 // 0x2C
    uint16_t Flag6;                 // 0x30
    uint16_t Flag7;                 // 0x32
    uint32_t aConstantPool;         // 0x34 - Constant pool address
};  // Size: 0x38 (56 bytes)
// Optional items (OptionalObjectInfo) follow
```

### tOptionalObjectInfo
Optional object information (if (tObject.ObjectType & 0x80) == 0x80).

```c
struct tOptionalObjectInfo {
    uint32_t fDesigner;             // 0x00 - Designer flag (2 = designer)
    uint32_t aObjectCLSID;          // 0x04 - Object CLSID address
    uint32_t Null1;                 // 0x08
    uint32_t aGuidObjectGUI;        // 0x0C - GUI GUID address
    uint32_t lObjectDefaultIIDCount;// 0x10 - Default IID count
    uint32_t aObjectEventsIIDTable; // 0x14 - Events IID table address
    uint32_t lObjectEventsIIDCount; // 0x18 - Events IID count
    uint32_t aObjectDefaultIIDTable;// 0x1C - Default IID table address
    uint32_t ControlCount;          // 0x20 - Control count
    uint32_t aControlArray;         // 0x24 - Control array address
    uint16_t iEventCount;           // 0x28 - Number of events
    uint16_t iPCodeCount;           // 0x2A - P-Code count
    uint16_t oInitializeEvent;      // 0x2C - Initialize event offset
    uint16_t oTerminateEvent;       // 0x2E - Terminate event offset
    uint32_t aEventLinkArray;       // 0x30 - Event link array address
    uint32_t aBasicClassObject;     // 0x34 - Basic class object pointer
    uint32_t Null3;                 // 0x38
    uint32_t Flag2;                 // 0x3C
};  // Size: 0x40 (64 bytes)
```

---

## P-Code Structures

### ProcDscInfo (CodeInfo)
Procedure descriptor information for P-Code.

```c
struct ProcDscInfo {
    uint32_t table;                 // 0x00 - Table address
    uint16_t field_4;               // 0x04
    uint16_t FrameSize;             // 0x06 - Stack frame size
    uint16_t ProcSize;              // 0x08 - Procedure size
    uint16_t field_A;               // 0x0A
    uint16_t field_C;               // 0x0C
    uint16_t field_E;               // 0x0E
    uint16_t field_10;              // 0x10
    uint16_t field_12;              // 0x12
    uint16_t field_14;              // 0x14
    uint16_t field_16;              // 0x16
    uint16_t field_18;              // 0x18
    uint16_t field_1A;              // 0x1A
    uint16_t Flag;                  // 0x1C - Flags
};  // Size: 0x1E (30 bytes)
```

### TableInfo (ObjectInfo)
Table information for P-Code procedures.

```c
struct TableInfo {
    uint32_t data0;                 // 0x00
    uint32_t Record;                // 0x04 - Record address
    uint32_t data8;                 // 0x08
    uint32_t data0C;                // 0x0C
    uint32_t data10;                // 0x10
    uint32_t Owner;                 // 0x14 - Owner object
    uint32_t rtl;                   // 0x18 - Runtime library
    uint32_t data1C;                // 0x1C
    uint32_t data20;                // 0x20
    uint32_t data24;                // 0x24
    uint16_t JmpCnt;                // 0x28 - Jump count
    uint16_t data2A;                // 0x2A
    uint32_t data2C;                // 0x2C
    uint32_t data30;                // 0x30
    uint32_t ConstPool;             // 0x34 - Constant pool address
};  // Size: 0x38 (56 bytes)
```

### tCodeInfo
Code information structure for native code.

```c
struct tCodeInfo {
    uint32_t aObjectInfo;           // 0x00 - Object info address
    uint16_t Flag1;                 // 0x04
    uint16_t Flag2;                 // 0x06
    uint16_t CodeLength;            // 0x08 - Code length
    uint32_t Flag3;                 // 0x0A
    uint16_t Flag4;                 // 0x0E
    uint16_t Null1;                 // 0x10
    uint32_t Flag5;                 // 0x12
    uint16_t Flag6;                 // 0x16
};  // Size: 0x18 (24 bytes)
```

### P-Code Data Stack Types
```c
#define S_NONE      0
#define S_BYTE      1
#define S_INTEGER   2
#define S_LONG      3
#define S_STRING    4
#define S_CURRENCY  5
#define S_SINGLE    6
#define S_DOUBLE    7
```

---

## COM/Component Structures

### tCOMRegData
COM registration data.

```c
struct tCOMRegData {
    uint32_t oRegInfo;              // 0x00 - Offset to COM Interfaces Info
    uint32_t oNTSProjectName;       // 0x04 - Offset to Project/Typelib Name
    uint32_t oNTSHelpDirectory;     // 0x08 - Offset to Help Directory
    uint32_t oNTSProjectDescription;// 0x0C - Offset to Project Description
    uint8_t uuidProjectClsId[16];   // 0x10 - CLSID of Project/Typelib
    uint32_t lTlbLcid;              // 0x20 - LCID of Type Library
    uint16_t iPadding1;             // 0x24
    uint16_t iTlbVerMajor;          // 0x26 - Typelib Major Version
    uint16_t iTlbVerMinor;          // 0x28 - Typelib Minor Version
    uint16_t iPadding2;             // 0x2A
    uint32_t lPadding3;             // 0x2C
};  // Size: 0x30 (48 bytes)
```

### tCOMRegInfo
COM registration information per object.

```c
struct tCOMRegInfo {
    uint32_t oNextObject;           // 0x00 - Offset to next COM object
    uint32_t oObjectName;           // 0x04 - Offset to object name
    uint32_t oObjectDescription;    // 0x08 - Offset to object description
    uint32_t lInstancing;           // 0x0C - Instancing mode
    uint32_t lObjectID;             // 0x10 - Current object ID
    uint8_t uuidObjectClsID[16];    // 0x14 - CLSID of object
    uint32_t fIsInterface;          // 0x24 - Specifies if next CLSID is valid
    uint32_t oObjectClsID;          // 0x28 - Offset to CLSID of object interface
    uint32_t oControlClsID;         // 0x2C - Offset to CLSID of control interface
    uint32_t fIsControl;            // 0x30 - Specifies if CLSID above is valid
    uint32_t lMiscStatus;           // 0x34 - OLEMISC flags
    uint8_t fClassType;             // 0x38 - Class type
    uint8_t fObjectType;            // 0x39 - Object type
    uint16_t iToolboxBitmap32;      // 0x3A - Control bitmap ID in toolbox
    uint16_t iDefaultIcon;          // 0x3C - Minimized icon of control window
    uint16_t fIsDesigner;           // 0x3E - Designer flag
    uint32_t oDesignerData;         // 0x40 - Offset to designer data
};  // Size: 0x44 (68 bytes)
```

**fObjectType Values:**
- `0x02` - Designer
- `0x10` - Class Module
- `0x20` - User Control (OCX)
- `0x80` - User Document

### tComponent
External component structure.

```c
struct tComponent {
    uint32_t StructLength;          // 0x00 - Structure length
    uint32_t oUuid;                 // 0x04 - UUID offset
    uint32_t l2;                    // 0x08
    uint32_t l3;                    // 0x0C
    uint32_t l4;                    // 0x10
    uint32_t l5;                    // 0x14
    uint32_t l6;                    // 0x18
    uint32_t GUIDoffset;            // 0x1C - GUID offset
    uint32_t GUIDlength;            // 0x20 - GUID length (-1 = no UUID)
    uint32_t l7;                    // 0x24
    uint32_t FileNameOffset;        // 0x28 - Filename offset
    uint32_t SourceOffset;          // 0x2C - Source offset
    uint32_t NameOffset;            // 0x30 - Name offset
};
```

### tGuiTable
GUI table structure.

```c
struct tGuiTable {
    uint32_t lStructSize;           // 0x00 - Total structure size
    uint8_t uuidObjectGUI[16];      // 0x04 - UUID of Object GUI
    uint32_t Unknown1;              // 0x14
    uint32_t Unknown2;              // 0x18
    uint32_t Unknown3;              // 0x1C
    uint32_t Unknown4;              // 0x20
    uint32_t lObjectID;             // 0x24 - Current Object ID
    uint32_t Unknown5;              // 0x28
    uint32_t fOLEMisc;              // 0x2C - OLEMisc flags
    uint8_t uuidObject[16];         // 0x30 - UUID of Object
    uint32_t Unknown6;              // 0x40
    uint32_t Unknown7;              // 0x44
    uint32_t aFormPointer;          // 0x48 - Pointer to GUI object info
    uint32_t Unknown8;              // 0x4C
};  // Size: 0x50 (80 bytes)
```

---

## Control & Event Structures

### tControl
Control structure for forms.

```c
struct tControl {
    uint16_t Flag1;                 // 0x00
    uint16_t EventCount;            // 0x02 - Event count
    uint32_t Flag2;                 // 0x04
    uint32_t aGUID;                 // 0x08 - GUID address
    uint16_t index;                 // 0x0C - Control index
    uint16_t Const1;                // 0x0E
    uint32_t Null1;                 // 0x10
    uint32_t Null2;                 // 0x14
    uint32_t aEventTable;           // 0x18 - Event table address
    uint8_t Flag3;                  // 0x1C
    uint8_t Const2;                 // 0x1D
    uint16_t Const3;                // 0x1E
    uint32_t aName;                 // 0x20 - Control name (NTS)
    uint16_t Index2;                // 0x24 - Index 2
    uint16_t Const1Copy;            // 0x26 - Copy of Const1
};  // Size: 0x28 (40 bytes)
```

### tEventTable
Event table structure.

```c
struct tEventTable {
    uint32_t Null1;                 // 0x00
    uint32_t aControl;              // 0x04 - Control address
    uint32_t aObjectInfo;           // 0x08 - ObjectInfo address
    uint32_t aQueryInterface;       // 0x0C - QueryInterface function
    uint32_t aAddRef;               // 0x10 - AddRef function
    uint32_t aRelease;              // 0x14 - Release function
    // Followed by: uint32_t aEventPointer[EventCount]
};  // Size: 0x18 + (EventCount * 4)
```

### tEventLink
Event link structure.

```c
struct tEventLink {
    uint16_t Const1;                // 0x00
    uint8_t CompileType;            // 0x02 - Compilation type
    uint32_t aEvent;                // 0x03 - Event address (unaligned!)
    uint8_t PushCmd;                // 0x07 - Push command
    uint32_t pushAddress;           // 0x08 - Push address
    uint8_t Const;                  // 0x0C
};  // Size: 0x0D (13 bytes)
```

### tEventPointer
Event pointer structure.

```c
struct tEventPointer {
    uint8_t Const1;                 // 0x00
    uint32_t Flag1;                 // 0x01 - Flags (unaligned!)
    uint32_t Const2;                // 0x05 - Constant
    uint8_t Const3;                 // 0x09
    uint32_t aEvent;                // 0x0A - Event address (unaligned!)
};  // Size: 0x0E (14 bytes)
```

---

## Import/Export Structures

### ImportTable
Import table entry.

```c
struct ImportTable {
    uint32_t LookUpRVA;             // 0x00 - Import lookup table RVA
    uint32_t TimeDateStamp;         // 0x04 - Time/date stamp
    uint32_t Chains;                // 0x08 - Forwarder chains
    uint32_t NameRVA;               // 0x0C - DLL name RVA
    uint32_t AddressTableRVA;       // 0x10 - Import address table RVA
};  // Size: 0x14 (20 bytes)
```

### ExternalTable
External library table entry.

```c
struct ExternalTable {
    uint32_t Flag;                  // 0x00 - Flags
    uint32_t aExternalLibrary;      // 0x04 - External library address
};  // Size: 0x08 (8 bytes)
```

### ExternalLibrary
External library descriptor.

```c
struct ExternalLibrary {
    uint32_t aLibraryName;          // 0x00 - Library name (NTS)
    uint32_t aLibraryFunction;      // 0x04 - Function name (NTS)
};  // Size: 0x08 (8 bytes)
```

### tProjectInfo2
Extended project information.

```c
struct tProjectInfo2 {
    uint32_t lNull1;                // 0x00
    uint32_t aObjectTable;          // 0x04 - Pointer to object table
    uint32_t lConst1;               // 0x08
    uint32_t lNull2;                // 0x0C
    uint32_t aObjectDescriptorTable;// 0x10 - Object descriptor table
    uint32_t lNull3;                // 0x14
    uint32_t aNTSPrjDescription;    // 0x18 - Project description (NTS)
    uint32_t aNTSPrjHelpFile;       // 0x1C - Project help file (NTS)
    uint32_t lConst2;               // 0x20
    uint32_t lHelpContextID;        // 0x24 - Help context ID
};  // Size: 0x28 (40 bytes)
```

---

## P-Code Instruction Format

### Argument Type Characters
These format specifiers are used in the P-Code opcode table:

**Informative (no bytes):**
- `>` - Put following hex in subsegments up to next offset
- `h` - Return hex output of following typechars
- `}` - End Procedure

**Argument Types (consume bytes):**
- `.` - Name of object at specified address (Long from datastream)
- `b` - Byte from datastream (formerly `�`)
- `%` - Integer (2 bytes) from datastream
- `&` - Long (4 bytes) from datastream
- `!` - Single (4-byte float) from datastream
- `a` - Argument reference (Int + typechar), from constant pool
- `c` - Control index (Int from datastream)
- `l` - Local variable reference (Int from datastream)
- `L` - Take N local variable references (N = Int from datastream)
- `m` - Local variable reference followed by typechar
- `n` - Hex Integer
- `o` - Item off stack (Pop)
- `p` - (Int from datastream) + Procedure Base Address
- `t` - Followed by typechar: `o`=ObjectName, `c`=control name
- `u` - Push (not used anymore)
- `v` - VTable reference (complex)
- `z` - Null-terminated Unicode string from file

**Type Characters:**
- `b` - Byte
- `?` - Boolean
- `%` - Integer (2 bytes)
- `!` - Single (4 bytes)
- `&` - Long (4 bytes)
- `~` - Variant
- `z` - String

### Opcode Prefixes
- `0x00-0xFA` - Single-byte opcodes
- `0xFB-0xFF` - Extended opcodes (followed by second byte)

**Extended opcode ranges:**
- `0xFB xx` - Opcode table 1
- `0xFC xx` - Opcode table 2
- `0xFD xx` - Opcode table 3
- `0xFE xx` - Opcode table 4
- `0xFF xx` - Opcode table 5

---

## Key P-Code Opcodes (Selected)

### Control Flow
- `0x1C` - BranchF %l (Branch if False)
- `0x1D` - BranchT %l (Branch if True)
- `0x1E` - Branch %l (Unconditional Branch)
- `0x13` - ExitProcHresult (Exit procedure with HRESULT)
- `0x14` - ExitProc (Exit procedure)
- `0x4B` - OnErrorGoto %l (Set up error handler)

### Stack Operations
- `0x1B` - LitStr %s (Push string literal)
- `0x28` - LitVarI2 %a,%2 (Push 2-byte integer variant)
- `0x3A` - LitVarStr %a %s (Push string variant)
- `0x63` - LitVar_TRUE (Push TRUE variant)
- `0x27` - LitVar_Missing (Push missing variant)

### Variable Operations
- `0x04` - FLdRfVar %a (Load reference to variable)
- `0x6B` - FLdI2 [%a] (Load 2-byte integer)
- `0x70` - FStI2 [%a] (Store 2-byte integer)
- `0x7F` - ILdI2 %a (Load 2-byte integer)
- `0x80` - ILdI4 %a (Load 4-byte integer)
- `0x84` - IStI2 %c (Store 2-byte integer)

### Function Calls
- `0x05` - ImpAdLdRf %c (Load import address reference)
- `0x09` - ImpAdCallHresult (Call external function - HRESULT)
- `0x0A` - ImpAdCallFPR4 %x (Call external function - Float)
- `0x5E` - ImpAdCallI4 %x (Call external function - Long)
- `0x0D` - VCallHresult %v (Call VTable method)

### String Operations
- `0x2A` - ConcatStr (Concatenate strings)
- `0x2F` - FFree1Str (Free single string)
- `0x32` - FFreeStr (Free multiple strings)
- `0x33` - LdFixedStr %s (Load fixed-length string)
- `0x34` - CStr2Ansi (Convert VB string to ANSI)
- `0x43` - FStStrCopy %a (Store copy of string)
- `0x4A` - FnLenStr (Get string length)

### Array Operations
- `0x3B` - Ary1StStrCopy (Store single array element)
- `0x40` - Ary1LdRf (Load array element reference)
- `0x41` - Ary1LdPr (Load array element pointer)
- `0x52` - Ary1StVar (Store variant to array)
- `0x5A` - Erase (Erase array)

### For/Next Loops
- `0x64` - NextI2 (End FOR/NEXT loop - Integer)
- `0x65` - NextStepI2 (End FOR/NEXT with step - Integer)
- `0x66` - NextI4 (End FOR/NEXT loop - Long)
- `0x67` - NextStepI4 (End FOR/NEXT with step - Long)
- `0x68` - NextStepR4 (End FOR/NEXT with step - Single)
- `0x69` - NextStepR8 (End FOR/NEXT with step - Double)

### Memory Management
- `0x29` - FFreeAd (Free multiple addresses)
- `0x1A` - FFree1Ad (Free single address)
- `0x35` - FFree1Var (Free single variant)
- `0x36` - FFreeVar (Free multiple variants)

---

## Notes

- **NTS** = Null-Terminated String
- **RVA** = Relative Virtual Address
- Many structures contain unaligned members (especially event structures)
- Addresses (`a` prefix) are absolute memory addresses
- Offsets (`o` prefix) are relative offsets that need base address added
- P-Code is stack-based and operates on a virtual stack
- VTable calls are COM interface method calls
- The constant pool contains literals and references used by P-Code

---

## References

1. Semi-VB-Decompiler: https://github.com/VBGAMER45/Semi-VB-Decompiler
   - Primary source for structure definitions
   - VB6 implementation with complete VB binary parser

2. See RESEARCH.md for additional references and implementation strategy

---

*Document generated from Semi-VB-Decompiler source code analysis*
*Last updated: 2026-02-20*
