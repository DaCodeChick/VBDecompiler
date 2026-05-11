// PE file format structures
// References: Microsoft PE/COFF specification

const std = @import("std");

// DOS Header (IMAGE_DOS_HEADER)
// Note: Not using packed struct due to array limitations in Zig 0.16
pub const DosHeader = extern struct {
    e_magic: u16, // Magic number (0x5A4D = "MZ")
    e_cblp: u16, // Bytes on last page of file
    e_cp: u16, // Pages in file
    e_crlc: u16, // Relocations
    e_cparhdr: u16, // Size of header in paragraphs
    e_minalloc: u16, // Minimum extra paragraphs needed
    e_maxalloc: u16, // Maximum extra paragraphs needed
    e_ss: u16, // Initial (relative) SS value
    e_sp: u16, // Initial SP value
    e_csum: u16, // Checksum
    e_ip: u16, // Initial IP value
    e_cs: u16, // Initial (relative) CS value
    e_lfarlc: u16, // File address of relocation table
    e_ovno: u16, // Overlay number
    e_res: [4]u16, // Reserved words
    e_oemid: u16, // OEM identifier
    e_oeminfo: u16, // OEM information
    e_res2: [10]u16, // Reserved words
    e_lfanew: i32, // File address of new exe header

    pub const MAGIC = 0x5A4D; // "MZ"
    pub const SIZE = @sizeOf(DosHeader);
};

// PE Signature
pub const PE_SIGNATURE = 0x00004550; // "PE\0\0"

// COFF File Header (IMAGE_FILE_HEADER)
pub const CoffHeader = extern struct {
    machine: u16,
    number_of_sections: u16,
    time_date_stamp: u32,
    pointer_to_symbol_table: u32,
    number_of_symbols: u32,
    size_of_optional_header: u16,
    characteristics: u16,

    pub const SIZE = @sizeOf(CoffHeader);

    // Machine types
    pub const MACHINE_I386 = 0x014c;
    pub const MACHINE_AMD64 = 0x8664;
    pub const MACHINE_ARM = 0x01c0;
    pub const MACHINE_ARM64 = 0xaa64;

    // Characteristics flags
    pub const IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002;
    pub const IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020;
    pub const IMAGE_FILE_32BIT_MACHINE = 0x0100;
    pub const IMAGE_FILE_DLL = 0x2000;
};

// Data Directory
pub const DataDirectory = extern struct {
    virtual_address: u32,
    size: u32,

    pub const SIZE = @sizeOf(DataDirectory);
};

// Optional Header (32-bit)
pub const OptionalHeader32 = extern struct {
    magic: u16,
    major_linker_version: u8,
    minor_linker_version: u8,
    size_of_code: u32,
    size_of_initialized_data: u32,
    size_of_uninitialized_data: u32,
    address_of_entry_point: u32,
    base_of_code: u32,
    base_of_data: u32,
    image_base: u32,
    section_alignment: u32,
    file_alignment: u32,
    major_operating_system_version: u16,
    minor_operating_system_version: u16,
    major_image_version: u16,
    minor_image_version: u16,
    major_subsystem_version: u16,
    minor_subsystem_version: u16,
    win32_version_value: u32,
    size_of_image: u32,
    size_of_headers: u32,
    check_sum: u32,
    subsystem: u16,
    dll_characteristics: u16,
    size_of_stack_reserve: u32,
    size_of_stack_commit: u32,
    size_of_heap_reserve: u32,
    size_of_heap_commit: u32,
    loader_flags: u32,
    number_of_rva_and_sizes: u32,

    pub const MAGIC_PE32 = 0x010b;
    pub const MAGIC_PE32PLUS = 0x020b;
    pub const SIZE = @sizeOf(OptionalHeader32);
    pub const NUM_DATA_DIRECTORIES = 16;

    // Subsystem values
    pub const IMAGE_SUBSYSTEM_WINDOWS_GUI = 2;
    pub const IMAGE_SUBSYSTEM_WINDOWS_CUI = 3;
};

// Section Header (IMAGE_SECTION_HEADER)
pub const SectionHeader = extern struct {
    name: [8]u8,
    virtual_size: u32,
    virtual_address: u32,
    size_of_raw_data: u32,
    pointer_to_raw_data: u32,
    pointer_to_relocations: u32,
    pointer_to_linenumbers: u32,
    number_of_relocations: u16,
    number_of_linenumbers: u16,
    characteristics: u32,

    pub const SIZE = @sizeOf(SectionHeader);

    // Section characteristics
    pub const IMAGE_SCN_CNT_CODE = 0x00000020;
    pub const IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040;
    pub const IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080;
    pub const IMAGE_SCN_MEM_EXECUTE = 0x20000000;
    pub const IMAGE_SCN_MEM_READ = 0x40000000;
    pub const IMAGE_SCN_MEM_WRITE = 0x80000000;

    pub fn getName(self: *const SectionHeader) []const u8 {
        const end = std.mem.indexOfScalar(u8, &self.name, 0) orelse self.name.len;
        return self.name[0..end];
    }
};

// Import Directory Entry
pub const ImportDirectoryEntry = extern struct {
    import_lookup_table_rva: u32,
    time_date_stamp: u32,
    forwarder_chain: u32,
    name_rva: u32,
    import_address_table_rva: u32,

    pub const SIZE = @sizeOf(ImportDirectoryEntry);
};

// Export Directory
pub const ExportDirectory = extern struct {
    characteristics: u32,
    time_date_stamp: u32,
    major_version: u16,
    minor_version: u16,
    name_rva: u32,
    base: u32,
    number_of_functions: u32,
    number_of_names: u32,
    address_of_functions: u32,
    address_of_names: u32,
    address_of_name_ordinals: u32,

    pub const SIZE = @sizeOf(ExportDirectory);
};

// Data directory indices
pub const DataDirectoryIndex = enum(u32) {
    export_table = 0,
    import_table = 1,
    resource_table = 2,
    exception_table = 3,
    certificate_table = 4,
    base_relocation_table = 5,
    debug = 6,
    architecture = 7,
    global_ptr = 8,
    tls_table = 9,
    load_config_table = 10,
    bound_import = 11,
    iat = 12,
    delay_import_descriptor = 13,
    com_descriptor = 14,
    reserved = 15,
};

// Helper to convert between RVA and file offset
pub fn rvaToOffset(rva: u32, sections: []const SectionHeader) ?u32 {
    for (sections) |section| {
        const section_start = section.virtual_address;
        const section_end = section_start + section.virtual_size;
        
        if (rva >= section_start and rva < section_end) {
            const offset = rva - section_start;
            return section.pointer_to_raw_data + offset;
        }
    }
    return null;
}

test "DOS header size" {
    try std.testing.expectEqual(64, DosHeader.SIZE);
}

test "COFF header size" {
    try std.testing.expectEqual(20, CoffHeader.SIZE);
}

test "Section header size" {
    try std.testing.expectEqual(40, SectionHeader.SIZE);
}
