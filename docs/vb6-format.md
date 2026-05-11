# Visual Basic 6 Binary File Structure - Comprehensive Research Summary

## Executive Summary

This document provides a comprehensive technical overview of Visual Basic 6 (VB6) binary file structure, compilation methods, runtime dependencies, and decompilation challenges. This information is essential for implementing a VB6 decompiler.

---

## 1. VB6 PE File Structure and Headers

### 1.1 PE File Format Basics
VB6 executables follow the standard Windows Portable Executable (PE) format with VB-specific extensions:

- **DOS Header** (0x00-0x3F): Standard MZ header
- **DOS Stub** (0x40-0x7F): "This program cannot be run in DOS mode"
- **Rich Signature** (0x80-Variable): Microsoft linker signature (undocumented)
- **PE Header** (Variable): NT Headers starting with "PE\0\0"
- **Optional Header**: Contains IMAGE_DATA_DIRECTORY entries
- **Section Headers**: .text, .data, .rsrc, etc.

### 1.2 VB5/VB6 Signature Detection

VB6 binaries contain specific signatures that identify them:

**VB5 Signature**: `VB5!` (ASCII: 0x56 0x42 0x35 0x21)
**VB6 Signature**: Located in the executable, can be found through:
- Import table references to MSVBVM60.DLL
- Presence of VB-specific structures
- ThunderRT Main entry point

**Key Identification Methods**:
```
1. Check imports for MSVBVM60.DLL or MSVBVM50.DLL
2. Scan for VB header structures
3. Look for "ThunderRTMain" or "ThunRTMain" function names
4. Check for VB-specific COM objects and interfaces
```

### 1.3 Rich Signature Structure

The Rich Signature (0x80 offset) contains:
- **DanS** dword at start (encrypted)
- Product ID and build count pairs
- **Rich** dword as terminator
- XOR mask following the Rich dword

Structure:
```
Offset 0x80:
- DanS (encrypted with XOR mask)
- 3x XOR mask values
- Multiple [ProductID, Count] pairs
- "Rich" signature (0x68636952)
- XOR mask (plaintext)
```

The XOR mask is generated from:
1. Checksum of first 0x80 bytes of PE
2. Checksum of compiler product IDs
3. Initial value of 0x80

---

## 2. P-Code vs Native Code Compilation

### 2.1 Compilation Modes

VB5 and VB6 support two compilation modes:

#### P-Code (Pseudo-Code)
- **Pros**:
  - Smaller executable size
  - Faster compilation
  - Better for prototyping
- **Cons**:
  - Slower runtime execution (interpreted)
  - Still requires MSVBVM60.DLL runtime
  - Not suitable for performance-critical apps

**P-Code Structure**:
- Bytecode instructions stored in CODE section
- Stack-based virtual machine
- Interpreted by MSVBVM60.DLL at runtime
- Opcodes typically 1-3 bytes

**Common P-Code Instructions**:
```
PushI    - Push integer
PushS    - Push string
PopI     - Pop integer
Call     - Call function
Ret      - Return
Add      - Addition
Sub      - Subtraction
Mul      - Multiplication
Div      - Division
```

#### Native Code
- **Pros**:
  - Much faster execution
  - Direct x86 machine code
  - Better performance for calculation-heavy apps
- **Cons**:
  - Larger file size
  - Still requires runtime libraries
  - Slower compilation

**Native Code Characteristics**:
- Standard x86 assembly instructions
- Can be disassembled with IDA Pro, Ghidra
- Still uses VB runtime for forms, controls, COM
- Optimizations available (Optimize for Fast Code/Small Code)

### 2.2 Detecting Compilation Mode

**P-Code Detection**:
- Check for VB P-Code interpreter calls
- Look for bytecode sequences in .text section
- Smaller .text section relative to executable size
- Presence of p-code instruction handlers

**Native Code Detection**:
- Standard x86 prologue/epilogue (push ebp; mov ebp, esp)
- Direct assembly instructions
- Larger .text section
- More complex disassembly

---

## 3. VB6 Runtime (MSVBVM60.DLL) Dependencies

### 3.1 Core Runtime Library

**MSVBVM60.DLL** (Microsoft Visual Basic Virtual Machine 6.0)
- **Size**: ~1.4 MB
- **Location**: %SystemRoot%\System32 (or SysWOW64 on 64-bit)
- **Purpose**: Core VB6 runtime engine

**Key Functions**:
```
ThunRTMain           - Main entry point
__vbaNew             - Object creation
__vbaFreeObj         - Object destruction
__vbaFreeStr         - String cleanup
__vbaFreeVar         - Variant cleanup
__vbaStrCopy         - String copy
__vbaVarDup          - Variant duplication
__vbaObjSet          - Object assignment
rtcMsgBox            - Message box
rtcInputBox          - Input box
```

### 3.2 Additional Runtime Dependencies

**Common DLLs**:
- **MSVBVM60.DLL**: Core runtime
- **STDOLE2.TLB**: OLE automation type library
- **OLEAUT32.DLL**: OLE automation
- **OLEPRO32.DLL**: OLE property pages
- **ASYCFILT.DLL**: Async filters
- **COMCAT.DLL**: Component categories manager

**Optional Components**:
- MSCOMCTL.OCX - Common controls
- COMDLG32.OCX - Common dialogs
- MSSTDFMT.DLL - Data formatting
- MSWINSCK.OCX - Winsock control
- RICHTX32.OCX - Rich text box
- TABCTL32.OCX - Tab control

### 3.3 Import Address Table (IAT)

VB6 binaries contain IAT entries for:
```
MSVBVM60.DLL functions:
  - ThunRTMain
  - DllFunctionCall
  - __vbaNew, __vbaFreeObj
  - __vbaVarMove, __vbaVarDup
  - rtcMsgBox, rtcInputBox
  
OLEAUT32.DLL functions:
  - SysAllocString
  - SysFreeString
  - VariantInit
  - VariantClear
  
KERNEL32.DLL functions:
  - GetModuleHandleA
  - GetProcAddress
  - LoadLibraryA
```

### 3.4 Runtime Thunks

**Thunks** are small code snippets that:
- Bridge VB code to Windows API
- Handle data type conversions
- Manage calling conventions (stdcall vs. cdecl)
- Provide error handling wrappers

**Thunk Structure**:
```assembly
; Example thunk for API call
push ebp
mov ebp, esp
push [ebp+08h]       ; Parameter
call [API_Address]   ; Call API
mov esp, ebp
pop ebp
ret 04h
```

---

## 4. Form Data (.frm) Embedded in Compiled Binaries

### 4.1 Form Storage Format

VB6 stores form definitions in a binary format within the executable:

**Form Header Structure**:
```c
struct VB_FORM_HEADER {
    DWORD dwSignature;      // 'Form' marker
    DWORD dwVersion;        // Form version
    DWORD dwFormFlags;      // Form properties flags
    DWORD dwControlCount;   // Number of controls
    DWORD dwFormDataSize;   // Size of form data
    DWORD dwFormDataOffset; // Offset to form data
};
```

### 4.2 Form Properties

Stored properties include:
- Form dimensions (Top, Left, Width, Height)
- Form caption/title
- Form icons
- BackColor, ForeColor
- BorderStyle
- ShowInTaskbar
- StartUpPosition
- WindowState (Normal, Minimized, Maximized)

### 4.3 Control Information

Each control stores:
```c
struct VB_CONTROL {
    WORD wControlType;      // Type ID (CommandButton, TextBox, etc.)
    WORD wControlID;        // Unique control ID
    DWORD dwNameOffset;     // Offset to control name
    DWORD dwCaptionOffset;  // Offset to caption text
    RECT rcPosition;        // Control position (L,T,W,H)
    DWORD dwProperties;     // Property bag offset
    DWORD dwEventHandlers;  // Event handler table offset
};
```

**Common Control Types**:
- 0x01: CommandButton
- 0x02: Label
- 0x03: TextBox
- 0x04: Frame
- 0x05: CheckBox
- 0x06: OptionButton
- 0x07: ComboBox
- 0x08: ListBox
- 0x09: PictureBox
- 0x0A: Timer

### 4.4 Binary Form Data Location

Forms are typically stored in:
1. **.rsrc section**: As FORM resources
2. **Custom VB section**: Some compilers create .vbfrm section
3. **Embedded in .data**: Form metadata in data section

**Extraction Process**:
```
1. Locate resource directory in PE
2. Find FORM resource type (RT_VBFORM)
3. Parse form header
4. Extract control array
5. Reconstruct form layout
```

---

## 5. VB6 Object Model

### 5.1 Module Types

**Standard Modules (.bas)**:
- Global variables
- Public/Private procedures
- No visual interface
- Compiled to standard functions

**Class Modules (.cls)**:
- Object-oriented classes
- Properties, methods, events
- Implements COM interfaces
- Can be instantiated

**Form Modules (.frm)**:
- Visual forms with controls
- Event-driven code
- Inherits from VB Form class
- Has associated .frx (binary data)

**UserControls (.ctl)**:
- Custom controls
- Can be embedded in forms
- Compiled to OCX files
- Similar structure to forms

### 5.2 Object Hierarchy

```
Application (App object)
  └─ Forms Collection
       ├─ Form1
       │    ├─ Controls Collection
       │    │    ├─ CommandButton1
       │    │    ├─ TextBox1
       │    │    └─ Label1
       │    └─ Event Handlers
       └─ Form2
  └─ Global Objects
       ├─ Standard Modules
       ├─ Class Modules
       └─ External Objects
```

### 5.3 VB Object Table (VB5/6)

The VB Object Table stores metadata:

```c
struct VB_OBJECT_TABLE {
    DWORD dwSignature;           // 'VB5!' or 'VB6!'
    DWORD dwRuntimeVersion;      // Runtime version
    DWORD dwProjectName;         // Offset to project name
    DWORD dwProjectDescription;  // Offset to description
    DWORD dwHelpFile;            // Help file path offset
    DWORD dwProjectHelpID;       // Help context ID
    DWORD dwThreadingModel;      // 0=Apartment, 1=STA
    DWORD dwFormCount;           // Number of forms
    DWORD dwFormTableOffset;     // Offset to form table
    DWORD dwExternalCount;       // External component count
    DWORD dwExternalTableOffset; // Offset to external table
    DWORD dwObjectTableOffset;   // COM object table
};
```

### 5.4 COM Object Structure

VB6 uses COM extensively:

**IUnknown Interface**:
```c
interface IUnknown {
    HRESULT QueryInterface(REFIID riid, void** ppv);
    ULONG AddRef();
    ULONG Release();
};
```

**IDispatch Interface** (for late binding):
```c
interface IDispatch : IUnknown {
    HRESULT GetTypeInfoCount(UINT* pctinfo);
    HRESULT GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo);
    HRESULT GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, 
                          UINT cNames, LCID lcid, DISPID* rgDispId);
    HRESULT Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                   WORD wFlags, DISPPARAMS* pDispParams,
                   VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
                   UINT* puArgErr);
};
```

---

## 6. String Tables and Resource Embedding

### 6.1 String Storage

VB6 uses **BSTR** (Basic String) format:

**BSTR Structure**:
```
[4-byte length][Unicode string data][2-byte null terminator]
 ^              ^
 |              +-- Pointer returned to application
 +-- Hidden length prefix
```

**Characteristics**:
- Length-prefixed
- Unicode (UTF-16LE) encoded
- NULL-terminated for C compatibility
- Allocated via SysAllocString()

**String Table Location**:
1. **.rdata section**: Read-only string constants
2. **.data section**: Mutable string buffers
3. **Resource section**: Form captions, labels

### 6.2 Resource Types

**Standard Resources**:
- **RT_ICON** (Type 3): Application icons
- **RT_BITMAP** (Type 2): Images
- **RT_STRING** (Type 6): String tables
- **RT_RCDATA** (Type 10): Binary data
- **RT_VERSION** (Type 16): Version info

**VB-Specific Resources**:
- **RT_VBFORM**: Form definitions
- **RT_VBCONTROL**: Control data
- **RT_VBRESOURCE**: VB-specific resources

### 6.3 Version Information

Located in RT_VERSION resource:

```c
struct VS_VERSIONINFO {
    WORD  wLength;
    WORD  wValueLength;
    WORD  wType;
    WCHAR szKey[16];          // "VS_VERSION_INFO"
    
    VS_FIXEDFILEINFO Value;
    
    // Variable-length strings:
    // - CompanyName
    // - FileDescription
    // - FileVersion
    // - InternalName
    // - LegalCopyright
    // - OriginalFilename
    // - ProductName
    // - ProductVersion
};
```

### 6.4 Extracting Resources

**Process**:
```
1. Parse PE headers
2. Locate IMAGE_DIRECTORY_ENTRY_RESOURCE
3. Parse resource directory tree
4. Extract resource data by type/name/language
5. Decode resource-specific formats
```

---

## 7. Known Challenges in VB6 Decompilation

### 7.1 Loss of High-Level Constructs

**Lost Information**:
- Variable names (replaced with generic names)
- Comments
- Original code formatting
- Developer intent
- High-level control flow (Do While vs. Do Until)

**Why**:
- VB compiler optimizes and transforms code
- Symbol information not stored in release builds
- Multiple source constructs compile to same output

### 7.2 P-Code Interpretation

**Challenges**:
- P-Code instruction set not publicly documented
- Different VB versions have different opcodes
- Stack-based operations hard to reverse
- Limited debugging symbols

**P-Code Complexity**:
```
Original VB:
  If x > 5 Then
    y = x * 2
  End If

P-Code (approximation):
  PUSH_VAR x
  PUSH_CONST 5
  CMP_GT
  JZ skip_block
  PUSH_VAR x
  PUSH_CONST 2
  MUL
  POP_VAR y
skip_block:
```

### 7.3 Native Code Optimization

**Issues**:
- Register allocation obscures variables
- Inlining eliminates function boundaries
- Dead code elimination removes logic
- Loop unrolling changes structure
- Compiler optimizations change semantics

**Example**:
```assembly
; Original: y = x * 2
mov eax, [x]
shl eax, 1        ; Optimized to bit shift
mov [y], eax
```

### 7.4 COM Object Resolution

**Problems**:
- Late-bound COM calls via IDispatch
- DISPIDs resolved at runtime
- Type information in external TLBs
- Variant data type ambiguity
- QueryInterface calls difficult to trace

**Late Binding Example**:
```vb
Dim obj As Object
Set obj = CreateObject("Excel.Application")
obj.Visible = True  ' DISPID resolved at runtime
```

### 7.5 Control Flow Obfuscation

**Natural Obfuscation**:
- Event-driven model (callbacks)
- Implicit form initialization
- Control array indexing
- DoEvents() causing re-entrancy
- Timer events creating race conditions

**Intentional Obfuscation**:
- Code encryption
- String obfuscation
- Control flow flattening
- API call indirection
- Packer/protector tools

### 7.6 String Decryption

**Encrypted Strings**:
- XOR encoding
- Base64 encoding
- Custom algorithms
- Dynamic decryption at runtime
- Encrypted resources

### 7.7 Form Recreation

**Challenges**:
- Binary form format not documented
- Control positioning algorithms
- Z-order of controls
- TabIndex calculation
- Control arrays
- Menu structures

### 7.8 External Dependencies

**Issues**:
- Third-party OCX/DLL files
- ActiveX controls
- External type libraries
- Database connections (DAO/RDO/ADO)
- COM server registrations

---

## 8. Existing Tools and Research

### 8.1 Commercial Tools

#### **VB Decompiler by DotFix Software**
- Website: https://www.vb-decompiler.org/
- **Capabilities**:
  - Disassemble P-Code and Native Code
  - Reconstruct forms with controls
  - Extract resources
  - Decompile to VB-like pseudocode
  - Support for VB5/VB6
- **Limitations**:
  - Not full source recovery
  - Pseudocode, not compilable VB
  - Expensive licensing
  - No public API

#### **VB Watch**
- Runtime debugger and analyzer
- Code instrumentation
- Performance profiling
- Limited decompilation features

### 8.2 Open Source Projects

#### **Semi-VB-Decompiler by VBGAMER45**
- GitHub: https://github.com/VBGAMER45/Semi-VB-Decompiler
- **Features**:
  - Visual Basic 6.0 source (VB6)
  - Partial decompilation
  - Form structure analysis
  - Educational resource
- **Status**: Last updated 2025
- **Limitations**: Partial implementation

#### **VB3 Decompiler**
- Historical tool for VB3
- Shows early VB decompilation techniques
- Source available in some archives

### 8.3 Academic Research

#### **OpenRCE Article: "Blacklight and FUTo"**
- **Key Points**:
  - Rootkit detection in VB
  - Process ID bruteforce (PIDB)
  - PspCidTable manipulation
  - Kernel object manipulation
- **Relevance**: Understanding VB runtime internals

#### **Rich Signature Analysis (NTCore)**
- **Website**: https://ntcore.com/files/richsign.htm
- **Content**:
  - Decrypting Rich signatures
  - Product ID interpretation
  - Linker version detection
  - XOR mask calculation
- **Usefulness**: Identifying compiler versions

### 8.4 Reverse Engineering Tools

#### **IDA Pro / Ghidra**
- Industry-standard disassemblers
- VB6 signatures available
- Plugin support for VB analysis
- Native code analysis

#### **PE Explorer**
- PE file analysis
- Resource extraction
- Import/Export viewing
- Dependency walker

#### **API Monitor**
- Monitor API calls at runtime
- Useful for understanding VB runtime behavior
- Can log MSVBVM60.DLL calls

#### **x64dbg / OllyDbg**
- Runtime debuggers
- Breakpoint on VB functions
- Memory inspection
- Call stack analysis

---

## 9. Technical Implementation Notes for Decompiler

### 9.1 Phase 1: Binary Analysis

**Steps**:
1. **Parse PE headers**
   - Validate PE signature
   - Read section headers
   - Locate IAT, resources
   
2. **Identify VB version**
   - Check MSVBVM**.DLL imports
   - Locate VB object table
   - Read version signature
   
3. **Determine compilation mode**
   - P-Code vs Native
   - Optimization level
   - Debug info presence

### 9.2 Phase 2: Resource Extraction

**Process**:
```python
def extract_resources(pe_file):
    resources = {}
    
    # Parse resource directory
    rsrc_dir = pe_file.DIRECTORY_ENTRY_RESOURCE
    
    for resource_type in rsrc_dir.entries:
        if resource_type.id == RT_VBFORM:
            # Extract form definitions
            forms = extract_forms(resource_type)
            resources['forms'] = forms
        elif resource_type.id == RT_STRING:
            # Extract string tables
            strings = extract_strings(resource_type)
            resources['strings'] = strings
        elif resource_type.id == RT_ICON:
            # Extract icons
            icons = extract_icons(resource_type)
            resources['icons'] = icons
    
    return resources
```

### 9.3 Phase 3: Code Analysis

**For P-Code**:
```
1. Locate p-code bytecode section
2. Disassemble using VB p-code instruction set
3. Build control flow graph
4. Reconstruct variable usage
5. Generate high-level pseudocode
```

**For Native Code**:
```
1. Use standard disassembler (Capstone, etc.)
2. Identify VB runtime function calls
3. Recognize VB patterns (object creation, etc.)
4. Reconstruct control flow
5. Pattern-match to VB constructs
```

### 9.4 Phase 4: Object Model Reconstruction

**Steps**:
1. **Parse VB Object Table**
   - Extract project info
   - Get form list
   - Get module list
   
2. **Reconstruct Forms**
   - Read form properties
   - Parse control arrays
   - Rebuild visual layout
   - Link event handlers
   
3. **Map Code to Events**
   - Identify event handler offsets
   - Match to form controls
   - Reconstruct event signatures

### 9.5 Phase 5: Decompilation

**Output Formats**:
1. **VB-like Pseudocode**
   - Human-readable
   - Close to original VB syntax
   - Non-compilable
   
2. **Intermediate Representation (IR)**
   - Machine-readable
   - Facilitates analysis
   - Can be transformed
   
3. **Form Files (.frm reconstructed)**
   - Visual form layouts
   - Control properties
   - Best-effort recreation

### 9.6 Key Algorithms

#### Control Flow Recovery
```
1. Build CFG from branch instructions
2. Identify loops (natural loops algorithm)
3. Detect conditionals (if-then-else patterns)
4. Recognize switch statements (jump tables)
5. Handle exceptions (On Error handlers)
```

#### Data Flow Analysis
```
1. Track variable definitions
2. Find variable uses (use-def chains)
3. Identify data types from runtime calls
4. Infer variable scope
5. Detect variable lifetime
```

#### Type Recovery
```
1. Analyze Variant operations
2. Track type through assignments
3. Use runtime function signatures
4. Infer from control properties
5. Use string patterns for guessing
```

---

## 10. Recommended Development Stack

### 10.1 Programming Language
- **Python 3.8+**: Rapid development, rich libraries
- **C++**: Performance-critical sections
- **Rust**: Modern, safe alternative

### 10.2 Core Libraries

**PE Parsing**:
- pefile (Python): https://github.com/erocarrera/pefile
- LIEF: https://lief.quarkslab.com/
- PE-bear: https://github.com/hasherezade/pe-bear-releases

**Disassembly**:
- Capstone: https://www.capstone-engine.org/
- Unicorn: https://www.unicorn-engine.org/
- radare2: https://rada.re/

**Decompilation**:
- RetDec: https://retdec.com/
- Snowman/SmartDec: https://derevenets.com/

**CFG/Analysis**:
- angr: https://angr.io/
- miasm: https://github.com/cea-sec/miasm
- Binary Ninja API: https://binary.ninja/

### 10.3 Development Tools
- IDA Pro (with Hex-Rays)
- Ghidra (free, NSA-developed)
- PE Explorer
- CFF Explorer
- Dependency Walker
- API Monitor

---

## 11. Further Reading

### Books
- *Reversing: Secrets of Reverse Engineering* by Eldad Eilam
- *Practical Malware Analysis* by Michael Sikorski
- *The IDA Pro Book* by Chris Eagle
- *Windows Internals* by Mark Russinovich

### Online Resources
- VB Decompiler Forum: http://www.vb-decompiler.org/forum/
- Semi-VB-Decompiler GitHub: https://github.com/VBGAMER45/Semi-VB-Decompiler
- OpenRCE: http://www.openrce.org/ (archived)
- NTCore: https://ntcore.com/

### Documentation
- MSDN Visual Basic 6.0 Documentation (archived)
- PE/COFF Specification: Microsoft Developer Network
- COM/OLE Automation Documentation
- VB6 Help Files (if available)

---

## 12. Legal and Ethical Considerations

### Permitted Uses
- Security research
- Malware analysis
- Legacy software migration
- Interoperability
- Educational purposes
- Recovering lost source code (own projects)

### Restricted Uses
- Piracy enablement
- Circumventing copy protection
- Reverse engineering for competition
- Violating license agreements
- Creating cracks/keygens

### Best Practices
- Always check local laws
- Respect software licenses
- Use for legitimate purposes
- Contribute to security community
- Document responsible disclosure

---

## Conclusion

VB6 decompilation is a complex but achievable task with proper understanding of:
1. PE file format and VB-specific structures
2. P-Code vs Native compilation differences
3. VB runtime architecture (MSVBVM60.DLL)
4. Form and control data formats
5. Object model and COM integration
6. Resource embedding techniques
7. Known challenges and limitations

This research provides a solid foundation for implementing a VB6 decompiler. The combination of static analysis, pattern recognition, and heuristics can achieve significant source code recovery, though perfect decompilation remains extremely difficult due to information loss during compilation.

**Key Success Factors**:
- Strong PE parsing capabilities
- P-Code disassembly engine
- Form reconstruction algorithms
- COM/OLE understanding
- Extensive pattern library
- Heuristic-based analysis
- Iterative improvement

Good luck with your VB6 decompiler implementation!

---

*Document compiled: May 11, 2026*  
*Sources: Microsoft documentation, reverse engineering research, open-source tools, academic papers*
