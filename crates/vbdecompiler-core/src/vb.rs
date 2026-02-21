// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! Visual Basic structure parsing module
//!
//! Parses VB5/6 specific structures within PE executables:
//! - VB header (VB5! signature)
//! - Project info
//! - Object table
//! - Method tables and P-Code

use crate::error::{Error, Result};
use crate::pe::PEFile;

/// VB5/6 Magic signature
const VB5_MAGIC: &[u8; 4] = b"VB5!";

/// VB5/6 Header structure (104 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBHeader {
    sz_vb_magic: [u8; 4],             // 0x00 - "VB5!" identifier
    w_runtime_build: u16,             // 0x04 - Runtime build number
    sz_language_dll: [u8; 14],        // 0x06 - Language DLL name
    sz_sec_language_dll: [u8; 14],    // 0x14 - Backup Language DLL
    w_runtime_dll_version: u16,       // 0x22 - Runtime DLL version
    dw_lcid: u32,                     // 0x24 - Language ID
    dw_sec_lcid: u32,                 // 0x28 - Backup Language ID
    lp_sub_main: u32,                 // 0x2C - Address to Sub Main()
    lp_project_info: u32,             // 0x30 - Address to Project Info
    f_mdl_int_objs: u32,              // 0x34 - MDL Internal Objects flag
    f_mdl_int_objs2: u32,             // 0x38 - MDL Internal Objects flag 2
    dw_thread_flags: u32,             // 0x3C - Thread flags
    dw_thread_count: u32,             // 0x40 - Thread count
    w_form_count: u16,                // 0x44 - Number of forms
    w_external_count: u16,            // 0x46 - External components count
    dw_thunk_count: u32,              // 0x48 - Thunk count
    lp_gui_table: u32,                // 0x4C - Address to GUI Table
    lp_external_component_table: u32, // 0x50 - External component table
    lp_com_register_data: u32,        // 0x54 - COM registration data
    b_sz_project_description: u32,    // 0x58 - Offset to project description
    b_sz_project_exe_name: u32,       // 0x5C - Offset to project EXE name
    b_sz_project_help_file: u32,      // 0x60 - Offset to help file
    b_sz_project_name: u32,           // 0x64 - Offset to project name
}

/// Project Information structure (564 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBProjectInfo {
    dw_version: u32,        // 0x00 - Signature/version
    lp_object_table: u32,   // 0x04 - Address to Object Table
    dw_null: u32,           // 0x08 - Null/Reserved
    lp_code_start: u32,     // 0x0C - Start of code address
    lp_code_end: u32,       // 0x10 - End of code address
    dw_data_size: u32,      // 0x14 - Data size
    lp_thread_space: u32,   // 0x18 - Thread space
    lp_vba_seh: u32,        // 0x1C - VBA exception handler
    lp_native_code: u32,    // 0x20 - Native code address
    sz_path1: [u8; 260],    // 0x24 - Path/Name
    sz_path2: [u8; 260],    // 0x128 - Secondary path
    lp_external_table: u32, // 0x22C - External table address
    dw_external_count: u32, // 0x230 - External count
}

/// Object Table Header (60 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBObjectTableHeader {
    lp_heap_link: u32,       // 0x00 - Heap link
    lp_exec_proj: u32,       // 0x04 - Execution project
    lp_project_info2: u32,   // 0x08 - Project info 2
    w_reserved: u16,         // 0x0C - Reserved
    w_total_objects: u16,    // 0x0E - Total number of objects
    w_compiled_objects: u16, // 0x10 - Compiled objects
    w_objects_in_use: u16,   // 0x12 - Objects in use
    lp_object_array: u32,    // 0x14 - Pointer to object array
    f_ide_flag: u32,         // 0x18 - IDE data flag
    f_ide_flag2: u32,        // 0x1C - IDE data flag 2
    lp_ide_data: u32,        // 0x20 - IDE data pointer
    lp_ide_data2: u32,       // 0x24 - IDE data pointer 2
    lp_sz_project_name: u32, // 0x28 - Project name pointer
    dw_lcid: u32,            // 0x2C - LCID
    dw_lcid2: u32,           // 0x30 - LCID 2
    lp_ide_data3: u32,       // 0x34 - IDE data pointer 3
    dw_identifier: u32,      // 0x38 - Template version
}

/// Public Object Descriptor (48 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBPublicObjectDescriptor {
    lp_object_info: u32,        // 0x00 - Object info pointer
    dw_reserved: u32,           // 0x04 - Reserved
    lp_public_bytes: u32,       // 0x08 - Public bytes pointer
    lp_static_bytes: u32,       // 0x0C - Static bytes pointer
    lp_module_public: u32,      // 0x10 - Module public pointer
    lp_module_static: u32,      // 0x14 - Module static pointer
    lp_sz_object_name: u32,     // 0x18 - Object name pointer
    dw_method_count: u32,       // 0x1C - Method count
    lp_method_names_array: u32, // 0x20 - Method names array pointer
    b_static_vars: u32,         // 0x24 - Static vars offset
    f_object_type: u32,         // 0x28 - Object type
    dw_null: u32,               // 0x2C - Null
}

/// Object Info structure (56 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBObjectInfo {
    w_ref_count: u16,       // 0x00 - Reference count
    w_object_index: u16,    // 0x02 - Object index
    lp_object_table: u32,   // 0x04 - Object table pointer
    lp_ide_data: u32,       // 0x08 - IDE data
    lp_private_object: u32, // 0x0C - Private object pointer
    dw_reserved: u32,       // 0x10 - Reserved
    dw_null: u32,           // 0x14 - Null
    lp_object: u32,         // 0x18 - Object pointer
    lp_project_data: u32,   // 0x1C - Project data
    w_method_count: u16,    // 0x20 - Method count
    w_method_count2: u16,   // 0x22 - Method count 2
    lp_methods: u32,        // 0x24 - Methods pointer
    w_constants: u16,       // 0x28 - Constants
    w_max_constants: u16,   // 0x2A - Max constants
    lp_ide_data2: u32,      // 0x2C - IDE data 2
    lp_ide_data3: u32,      // 0x30 - IDE data 3
    lp_constants: u32,      // 0x34 - Constants pointer
}

/// Optional Object Information (64 bytes) - for forms
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBOptionalObjectInfo {
    dw_designer_flag: u32,      // 0x00 - Designer flag
    lp_object_clsid: u32,       // 0x04 - Object CLSID pointer
    dw_null1: u32,              // 0x08 - Null
    lp_guid_object_gui: u32,    // 0x0C - GUI GUID pointer
    dw_default_iid_count: u32,  // 0x10 - Default IID count
    lp_events_iid_table: u32,   // 0x14 - Events IID table pointer
    dw_events_iid_count: u32,   // 0x18 - Events IID count
    lp_default_iid_table: u32,  // 0x1C - Default IID table pointer
    dw_control_count: u32,      // 0x20 - Control count
    lp_control_array: u32,      // 0x24 - Control array pointer
    w_event_count: u16,         // 0x28 - Number of events
    w_pcode_count: u16,         // 0x2A - P-Code count
    w_initialize_event: u16,    // 0x2C - Initialize event offset
    w_terminate_event: u16,     // 0x2E - Terminate event offset
    lp_event_link_array: u32,   // 0x30 - Event link array pointer
    lp_basic_class_object: u32, // 0x34 - Basic class object pointer
    dw_null2: u32,              // 0x38 - Null
    dw_flags: u32,              // 0x3C - Flags
}

/// Procedure Descriptor Information (30 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBProcDescInfo {
    lp_table: u32,     // 0x00 - Table pointer
    w_reserved1: u16,  // 0x04 - Reserved
    w_frame_size: u16, // 0x06 - Stack frame size
    w_proc_size: u16,  // 0x08 - Procedure size in bytes
    w_reserved2: u16,  // 0x0A - Reserved
    w_reserved3: u16,  // 0x0C - Reserved
    w_reserved4: u16,  // 0x0E - Reserved
    w_reserved5: u16,  // 0x10 - Reserved
    w_reserved6: u16,  // 0x12 - Reserved
    w_reserved7: u16,  // 0x14 - Reserved
    w_reserved8: u16,  // 0x16 - Reserved
    w_reserved9: u16,  // 0x18 - Reserved
    w_reserved10: u16, // 0x1A - Reserved
    w_flags: u16,      // 0x1C - Flags
}

/// Method Name Entry (8 bytes)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct VBMethodName {
    lp_method_name: u32, // 0x00 - Method name pointer
    dw_flags: u32,       // 0x04 - Flags
}

/// High-level VB Object representation
#[derive(Debug, Clone)]
pub struct VBObject {
    pub name: String,
    pub object_index: u32,
    pub object_type: u32,
    pub method_names: Vec<String>,
    descriptor: VBPublicObjectDescriptor,
    info: Option<VBObjectInfo>,
    optional_info: Option<VBOptionalObjectInfo>,
}

impl VBObject {
    /// Check if this is a form
    pub fn is_form(&self) -> bool {
        (self.object_type & 0x10) != 0
    }

    /// Check if this is a module
    pub fn is_module(&self) -> bool {
        (self.object_type & 0x01) != 0
    }

    /// Check if this is a class
    pub fn is_class(&self) -> bool {
        (self.object_type & 0x02) != 0
    }

    /// Check if this object has optional info
    pub fn has_optional_info(&self) -> bool {
        (self.object_type & 0x80) != 0
    }

    /// Get the method count
    pub fn method_count(&self) -> usize {
        self.method_names.len()
    }
}

/// VB file parser
pub struct VBFile {
    pe_file: PEFile,
    vb_header_rva: u32,
    vb_header: Option<VBHeader>,
    project_info: Option<VBProjectInfo>,
    object_table_header: Option<VBObjectTableHeader>,
    objects: Vec<VBObject>,
    is_native_code: bool,
}

impl VBFile {
    /// Parse VB structures from a PE file
    pub fn from_pe(pe_file: PEFile) -> Result<Self> {
        let mut vb_file = Self {
            pe_file,
            vb_header_rva: 0,
            vb_header: None,
            project_info: None,
            object_table_header: None,
            objects: Vec::new(),
            is_native_code: false,
        };

        vb_file.parse()?;
        Ok(vb_file)
    }

    /// Parse all VB structures
    fn parse(&mut self) -> Result<()> {
        // Find VB5! header
        self.find_vb_header()?;

        // Parse VB header
        self.parse_vb_header()?;

        // Parse project info
        self.parse_project_info()?;

        // Parse object table
        self.parse_object_table()?;

        // Parse all objects
        self.parse_objects()?;

        Ok(())
    }

    /// Find the VB5! signature in the PE file
    fn find_vb_header(&mut self) -> Result<()> {
        // Search for "VB5!" signature in all sections
        for section in self.pe_file.sections() {
            let start_rva = section.virtual_address;

            // Read section data
            if let Some(data) = self
                .pe_file
                .read_at_rva(start_rva, section.virtual_size as usize)
            {
                // Search for VB5! signature
                for i in 0..data.len().saturating_sub(4) {
                    if &data[i..i + 4] == VB5_MAGIC {
                        self.vb_header_rva = start_rva + i as u32;
                        return Ok(());
                    }
                }
            }
        }

        Err(Error::invalid_vb("VB5! signature not found"))
    }

    /// Parse the VB header
    fn parse_vb_header(&mut self) -> Result<()> {
        let header = self.read_struct::<VBHeader>(self.vb_header_rva)?;

        // Validate signature
        if &header.sz_vb_magic != VB5_MAGIC {
            return Err(Error::invalid_vb("Invalid VB header signature"));
        }

        self.vb_header = Some(header);
        Ok(())
    }

    /// Parse the project info structure
    fn parse_project_info(&mut self) -> Result<()> {
        let vb_header = self
            .vb_header
            .as_ref()
            .ok_or_else(|| Error::invalid_vb("VB header not parsed"))?;

        if vb_header.lp_project_info == 0 {
            return Err(Error::invalid_vb("No project info pointer in VB header"));
        }

        let project_info_rva = self.va_to_rva(vb_header.lp_project_info);
        let project_info = self.read_struct::<VBProjectInfo>(project_info_rva)?;

        // Determine if P-Code or native
        self.is_native_code = project_info.lp_native_code != 0;

        self.project_info = Some(project_info);
        Ok(())
    }

    /// Parse the object table
    fn parse_object_table(&mut self) -> Result<()> {
        let project_info = self
            .project_info
            .as_ref()
            .ok_or_else(|| Error::invalid_vb("Project info not parsed"))?;

        if project_info.lp_object_table == 0 {
            return Err(Error::invalid_vb("No object table pointer in project info"));
        }

        let object_table_rva = self.va_to_rva(project_info.lp_object_table);
        let object_table_header = self.read_struct::<VBObjectTableHeader>(object_table_rva)?;

        self.object_table_header = Some(object_table_header);
        Ok(())
    }

    /// Parse all objects (forms, modules, classes)
    fn parse_objects(&mut self) -> Result<()> {
        let object_table_header = self
            .object_table_header
            .as_ref()
            .ok_or_else(|| Error::invalid_vb("Object table header not parsed"))?;

        if object_table_header.w_total_objects == 0 {
            return Ok(()); // No objects
        }

        let object_array_rva = self.va_to_rva(object_table_header.lp_object_array);

        // Parse each object descriptor
        for i in 0..object_table_header.w_total_objects {
            let obj_rva =
                object_array_rva + (i as u32 * size_of::<VBPublicObjectDescriptor>() as u32);

            if let Ok(descriptor) = self.read_struct::<VBPublicObjectDescriptor>(obj_rva) {
                if let Ok(obj) = self.parse_object(descriptor, i as u32) {
                    self.objects.push(obj);
                }
            }
        }

        Ok(())
    }

    /// Parse a single object
    fn parse_object(&self, descriptor: VBPublicObjectDescriptor, index: u32) -> Result<VBObject> {
        let mut obj = VBObject {
            name: String::new(),
            object_index: index,
            object_type: descriptor.f_object_type,
            method_names: Vec::new(),
            descriptor,
            info: None,
            optional_info: None,
        };

        // Parse object name
        if descriptor.lp_sz_object_name != 0 {
            obj.name = self
                .read_string_at_rva(self.va_to_rva(descriptor.lp_sz_object_name), 256)
                .unwrap_or_else(|| format!("<Object{}>", index));
        } else {
            obj.name = format!("<Object{}>", index);
        }

        // Parse object info
        if descriptor.lp_object_info != 0 {
            let info_rva = self.va_to_rva(descriptor.lp_object_info);
            if let Ok(info) = self.read_struct::<VBObjectInfo>(info_rva) {
                obj.info = Some(info);

                // Parse optional info if present
                if (descriptor.f_object_type & 0x80) != 0 {
                    let opt_info_rva = info_rva + size_of::<VBObjectInfo>() as u32;
                    if let Ok(opt_info) = self.read_struct::<VBOptionalObjectInfo>(opt_info_rva) {
                        obj.optional_info = Some(opt_info);
                    }
                }
            }
        }

        // Parse method names
        self.parse_method_names(&mut obj)?;

        Ok(obj)
    }

    /// Parse method names for an object
    fn parse_method_names(&self, obj: &mut VBObject) -> Result<()> {
        if obj.descriptor.dw_method_count == 0 || obj.descriptor.lp_method_names_array == 0 {
            return Ok(());
        }

        let names_array_rva = self.va_to_rva(obj.descriptor.lp_method_names_array);

        for i in 0..obj.descriptor.dw_method_count {
            let entry_rva = names_array_rva + (i * size_of::<VBMethodName>() as u32);

            if let Ok(name_entry) = self.read_struct::<VBMethodName>(entry_rva) {
                if name_entry.lp_method_name != 0 {
                    let method_name = self
                        .read_string_at_rva(self.va_to_rva(name_entry.lp_method_name), 256)
                        .unwrap_or_else(|| format!("<Method{}>", i));
                    obj.method_names.push(method_name);
                } else {
                    obj.method_names.push(format!("<Method{}>", i));
                }
            } else {
                obj.method_names.push(format!("<Method{}>", i));
            }
        }

        Ok(())
    }

    /// Read a structure at an RVA
    fn read_struct<T: Copy>(&self, rva: u32) -> Result<T> {
        let size = size_of::<T>();
        let data = self.pe_file.read_at_rva(rva, size).ok_or_else(|| {
            Error::invalid_vb(format!("Failed to read structure at RVA 0x{:X}", rva))
        })?;

        if data.len() < size {
            return Err(Error::invalid_vb(format!(
                "Insufficient data at RVA 0x{:X}: expected {} bytes, got {}",
                rva,
                size,
                data.len()
            )));
        }

        // SAFETY: We've verified the size matches and T is Copy.
        // The packed repr ensures no alignment issues.
        unsafe { Ok(std::ptr::read_unaligned(data.as_ptr() as *const T)) }
    }

    /// Read a null-terminated string at an RVA
    fn read_string_at_rva(&self, rva: u32, max_length: usize) -> Option<String> {
        let data = self.pe_file.read_at_rva(rva, max_length)?;

        let null_pos = data.iter().position(|&b| b == 0)?;
        let string_data = &data[..null_pos];

        String::from_utf8(string_data.to_vec()).ok()
    }

    /// Convert Virtual Address to Relative Virtual Address
    fn va_to_rva(&self, va: u32) -> u32 {
        va.saturating_sub(self.pe_file.image_base())
    }

    /// Check if this is a valid VB file
    pub fn is_valid(&self) -> bool {
        self.vb_header.is_some()
    }

    /// Check if compiled to P-Code
    pub fn is_pcode(&self) -> bool {
        !self.is_native_code && self.vb_header.is_some()
    }

    /// Check if compiled to native code
    pub fn is_native_code(&self) -> bool {
        self.is_native_code && self.vb_header.is_some()
    }

    /// Get all parsed objects
    pub fn objects(&self) -> &[VBObject] {
        &self.objects
    }

    /// Get object by index
    pub fn object(&self, index: usize) -> Option<&VBObject> {
        self.objects.get(index)
    }

    /// Get object by name
    pub fn object_by_name(&self, name: &str) -> Option<&VBObject> {
        self.objects.iter().find(|obj| obj.name == name)
    }

    /// Get P-Code bytes for a specific method
    pub fn get_pcode_for_method(
        &self,
        object_index: usize,
        method_index: usize,
    ) -> Option<Vec<u8>> {
        if !self.is_pcode() {
            return None;
        }

        let obj = self.objects.get(object_index)?;
        let info = obj.info.as_ref()?;

        if info.lp_methods == 0 || method_index >= info.w_method_count as usize {
            return None;
        }

        // Read procedure descriptor
        let method_table_rva = self.va_to_rva(info.lp_methods);
        let proc_desc_rva =
            method_table_rva + (method_index as u32 * size_of::<VBProcDescInfo>() as u32);

        let proc_desc = self.read_struct::<VBProcDescInfo>(proc_desc_rva).ok()?;

        if proc_desc.w_proc_size == 0 {
            return None;
        }

        // P-Code follows the descriptor
        let pcode_rva = proc_desc_rva + size_of::<VBProcDescInfo>() as u32;
        let pcode_bytes = self
            .pe_file
            .read_at_rva(pcode_rva, proc_desc.w_proc_size as usize)?;

        Some(pcode_bytes.to_vec())
    }

    /// Get the underlying PE file
    pub fn pe_file(&self) -> &PEFile {
        &self.pe_file
    }

    /// Get project name if available
    pub fn project_name(&self) -> Option<String> {
        let vb_header = self.vb_header.as_ref()?;

        // Try bSZProjectName from VB header
        if vb_header.b_sz_project_name != 0 {
            if let Some(name) =
                self.read_string_at_rva(self.va_to_rva(vb_header.b_sz_project_name), 256)
            {
                if !name.is_empty() {
                    return Some(name);
                }
            }
        }

        // Try path from project info
        if let Some(project_info) = &self.project_info {
            if project_info.sz_path1[0] != 0 {
                if let Ok(path) = std::str::from_utf8(&project_info.sz_path1) {
                    let path_str = path.trim_end_matches('\0');
                    if !path_str.is_empty() {
                        return Some(path_str.to_string());
                    }
                }
            }
        }

        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vb5_magic() {
        assert_eq!(VB5_MAGIC, b"VB5!");
    }

    #[test]
    fn test_struct_sizes() {
        use std::mem::size_of;

        assert_eq!(size_of::<VBHeader>(), 104);
        assert_eq!(size_of::<VBProjectInfo>(), 564);
        assert_eq!(size_of::<VBObjectTableHeader>(), 60);
        assert_eq!(size_of::<VBPublicObjectDescriptor>(), 48);
        assert_eq!(size_of::<VBObjectInfo>(), 56);
        assert_eq!(size_of::<VBOptionalObjectInfo>(), 64);
        assert_eq!(size_of::<VBProcDescInfo>(), 30);
        assert_eq!(size_of::<VBMethodName>(), 8);
    }
}
