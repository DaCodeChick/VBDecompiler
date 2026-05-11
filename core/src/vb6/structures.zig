// VB6 binary structures and constants
const std = @import("std");

// VB6 runtime DLL names
pub const VB6_RUNTIME_DLL = "MSVBVM60.DLL";
pub const VB5_RUNTIME_DLL = "MSVBVM50.DLL";

// Known VB6 entry point names
pub const VB6_ENTRY_NAMES = [_][]const u8{
    "ThunRTMain",
    "DllFunctionCall",
    "_vbaVarMove",
    "_adj_fdiv_m32",
};

// VB Object Table structure (simplified)
pub const VBHeader = extern struct {
    signature: [4]u8, // "VB5!" or similar
    runtime_build: u16,
    language_dll: [14]u8,
    secondary_language_dll: [14]u8,
    runtime_dll_version: u16,
    language_id: u32,
    secondary_language_id: u32,
    
    pub const VB5_SIGNATURE = [4]u8{ 'V', 'B', '5', '!' };
    pub const VB6_SIGNATURE = [4]u8{ 'V', 'B', '6', '!' };
};

// VB compilation type
pub const CompilationType = enum {
    native,
    pcode,
    unknown,
};

// VB binary type
pub const BinaryType = enum {
    exe,
    dll,
    ocx,
    unknown,
};

// VB6 project information
pub const ProjectInfo = struct {
    name: []const u8,
    exe_name: []const u8,
    description: []const u8,
    help_file: []const u8,
    
    pub fn deinit(self: *ProjectInfo, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
        allocator.free(self.exe_name);
        allocator.free(self.description);
        allocator.free(self.help_file);
    }
};

// VB Object descriptor
pub const VBObjectInfo = struct {
    address: u32,
    size: u32,
    object_type: VBObjectType,
    name: []const u8,
    
    pub fn deinit(self: *VBObjectInfo, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
    }
};

pub const VBObjectType = enum {
    standard_module, // .bas
    class_module, // .cls
    form, // .frm
    user_control, // .ctl
    property_page,
    user_document,
    unknown,
};

// Form header structure (simplified)
pub const FormHeader = extern struct {
    signature: u32, // Form signature
    version: u32,
    checksum: u32,
    flags: u32,
    form_position_left: i32,
    form_position_top: i32,
    form_position_width: i32,
    form_position_height: i32,
};

// Control information
pub const ControlInfo = struct {
    control_type: []const u8,
    name: []const u8,
    left: i32,
    top: i32,
    width: i32,
    height: i32,
    properties: std.StringHashMap([]const u8),
    
    pub fn deinit(self: *ControlInfo, allocator: std.mem.Allocator) void {
        allocator.free(self.control_type);
        allocator.free(self.name);
        
        var it = self.properties.iterator();
        while (it.next()) |entry| {
            allocator.free(entry.key_ptr.*);
            allocator.free(entry.value_ptr.*);
        }
        self.properties.deinit();
    }
};

// String resource
pub const StringResource = struct {
    id: u32,
    rva: u32,
    value: []const u8,
    
    pub fn deinit(self: *StringResource, allocator: std.mem.Allocator) void {
        allocator.free(self.value);
    }
};

// Import information
pub const ImportInfo = struct {
    dll_name: []const u8,
    function_name: []const u8,
    ordinal: ?u16,
    address: u32,
    
    pub fn deinit(self: *ImportInfo, allocator: std.mem.Allocator) void {
        allocator.free(self.dll_name);
        allocator.free(self.function_name);
    }
};

// Export information
pub const ExportInfo = struct {
    name: []const u8,
    ordinal: u16,
    address: u32,
    forwarded_name: ?[]const u8,
    
    pub fn deinit(self: *ExportInfo, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
        if (self.forwarded_name) |fwd| {
            allocator.free(fwd);
        }
    }
};
