// VB6 Runtime Object Parser
// Parses VB Object Table and reconstructs project structure

const std = @import("std");
const pe = @import("../pe/parser.zig");
const structures = @import("structures.zig");

/// VB Object Table Header
pub const VBObjectTable = extern struct {
    signature: u32, // 'VB5!' = 0x21354256
    runtime_version: u16,
    language_dll: [14]u8,
    secondary_language_dll: [14]u8,
    runtime_dll_version: u16,
    language_id: u32,
    secondary_language_id: u32,
    next_object: u32, // Pointer to next object
    exec_code_offset: u32, // Offset to executable code
    project_data: u32, // Pointer to project data structure
    reserved1: u32,
    reserved2: u32,
    reserved3: u32,
    external_component_table: u32,
    object_table: u32, // Pointer to object descriptors
    
    pub const VB5_SIGNATURE: u32 = 0x21354256; // "VB5!"
    pub const VB6_SIGNATURE: u32 = 0x21364256; // "VB6!"
};

/// Project Information Structure
pub const VBProjectInfo = packed struct {
    signature: u32, // 'PROJ'
    version: u32,
    object_table_offset: u32,
    reserved1: u32,
    project_name_offset: u32,
    project_description_offset: u32,
    help_file_offset: u32,
    project_help_id: u32,
    project_lcid: u32,
    reserved2: u32,
    project_name_length: u32,
    project_description_length: u32,
    help_file_length: u32,
    project_path_offset: u32,
    project_path_length: u32,
};

/// Object Descriptor Entry
pub const VBObjectDescriptor = packed struct {
    next_object: u32, // Pointer to next object
    object_id: u32, // Unique object ID
    reserved1: u32,
    type_info: u32, // Pointer to type information
    reserved2: u32,
    reserved3: u32,
    name_offset: u32, // Offset to object name
    method_count: u32, // Number of methods
    method_names_offset: u32, // Offset to method names table
    static_vars_count: u32, // Number of static variables
    object_size: u32, // Size of object instance
    const_count: u32, // Number of constants
};

/// External Component Reference
pub const VBExternalComponent = packed struct {
    guid: [16]u8, // COM GUID
    version_major: u16,
    version_minor: u16,
    lcid: u32, // Locale ID
    reserved1: u32,
    reserved2: u32,
    name_offset: u32,
    name_length: u32,
};

/// VB Runtime Parser
pub const RuntimeParser = struct {
    allocator: std.mem.Allocator,
    pe_file: *const pe.PEFile,
    
    pub fn init(allocator: std.mem.Allocator, pe_file: *const pe.PEFile) RuntimeParser {
        return RuntimeParser{
            .allocator = allocator,
            .pe_file = pe_file,
        };
    }
    
    /// Find VB Object Table in PE file
    pub fn findObjectTable(self: *RuntimeParser) ?u32 {
        // Strategy 1: Look for VB signature in .data section
        const data_section_opt = blk: {
            for (self.pe_file.sections) |*section| {
                const name = section.getName();
                if (std.mem.eql(u8, name, ".data")) {
                    break :blk section;
                }
            }
            break :blk null;
        };
        
        const data_section = data_section_opt orelse return null;
        
        // Get section data
        const section_rva = data_section.virtual_address;
        const section_size = data_section.size_of_raw_data;
        const data = self.pe_file.rvaToData(section_rva, section_size) orelse return null;
        
        // Scan for VB5! or VB6! signature
        var i: usize = 0;
        while (i + 4 <= data.len) : (i += 4) {
            const sig = std.mem.readInt(u32, data[i..][0..4], .little);
            if (sig == VBObjectTable.VB5_SIGNATURE or sig == VBObjectTable.VB6_SIGNATURE) {
                return @intCast(section_rva + i);
            }
        }
        
        return null;
    }
    
    /// Parse VB Object Table
    pub fn parseObjectTable(self: *RuntimeParser, rva: u32) !VBObjectTable {
        const data = self.pe_file.rvaToData(rva, @sizeOf(VBObjectTable)) orelse return error.InvalidRVA;
        
        if (data.len < @sizeOf(VBObjectTable)) {
            return error.InsufficientData;
        }
        
        var table: VBObjectTable = undefined;
        @memcpy(std.mem.asBytes(&table), data[0..@sizeOf(VBObjectTable)]);
        
        return table;
    }
    
    /// Parse Project Information
    pub fn parseProjectInfo(self: *RuntimeParser, rva: u32) !structures.ProjectInfo {
        const data = self.pe_file.rvaToData(rva, @sizeOf(VBProjectInfo)) orelse return error.InvalidRVA;
        
        if (data.len < @sizeOf(VBProjectInfo)) {
            return error.InsufficientData;
        }
        
        var proj_info: VBProjectInfo = undefined;
        @memcpy(std.mem.asBytes(&proj_info), data[0..@sizeOf(VBProjectInfo)]);
        
        // Extract strings
        const name = try self.extractString(proj_info.project_name_offset, proj_info.project_name_length);
        const description = try self.extractString(proj_info.project_description_offset, proj_info.project_description_length);
        const help_file = try self.extractString(proj_info.help_file_offset, proj_info.help_file_length);
        
        return structures.ProjectInfo{
            .name = name,
            .exe_name = try self.allocator.dupe(u8, ""),
            .description = description,
            .help_file = help_file,
        };
    }
    
    /// Extract string from RVA
    fn extractString(self: *RuntimeParser, rva: u32, length: u32) ![]const u8 {
        if (rva == 0 or length == 0) {
            return try self.allocator.dupe(u8, "");
        }
        
        const data = self.pe_file.rvaToData(rva, length) orelse return error.InvalidRVA;
        
        const str_len = @min(length, data.len);
        return try self.allocator.dupe(u8, data[0..str_len]);
    }
    
    /// Parse object descriptors
    pub fn parseObjects(self: *RuntimeParser, first_object_rva: u32) !std.ArrayList(structures.VBObjectInfo) {
        var objects = std.ArrayList(structures.VBObjectInfo).empty;
        
        var current_rva = first_object_rva;
        while (current_rva != 0) {
            const obj_data = self.pe_file.rvaToData(current_rva, @sizeOf(VBObjectDescriptor)) orelse break;
            
            if (obj_data.len < @sizeOf(VBObjectDescriptor)) {
                break;
            }
            
            var obj_desc: VBObjectDescriptor = undefined;
            @memcpy(std.mem.asBytes(&obj_desc), obj_data[0..@sizeOf(VBObjectDescriptor)]);
            
            // Extract object name
            const name = try self.extractString(obj_desc.name_offset, 256);
            
            const obj_info = structures.VBObjectInfo{
                .address = current_rva,
                .size = obj_desc.object_size,
                .object_type = .unknown,
                .name = name,
            };
            
            try objects.append(self.allocator, obj_info);
            
            // Move to next object
            current_rva = obj_desc.next_object;
            
            // Safety check to prevent infinite loops
            if (objects.items.len > 1000) {
                break;
            }
        }
        
        return objects;
    }
    
    /// Parse external components (OCX, DLL references)
    pub fn parseExternalComponents(self: *RuntimeParser, table_rva: u32) !std.ArrayList(VBExternalComponent) {
        var components = std.ArrayList(VBExternalComponent).empty;
        
        var current_rva = table_rva;
        var count: usize = 0;
        
        while (count < 100) : (count += 1) { // Max 100 components
            const comp_data = self.pe_file.rvaToData(current_rva, @sizeOf(VBExternalComponent)) orelse break;
            
            if (comp_data.len < @sizeOf(VBExternalComponent)) {
                break;
            }
            
            var component: VBExternalComponent = undefined;
            @memcpy(std.mem.asBytes(&component), comp_data[0..@sizeOf(VBExternalComponent)]);
            
            // Check for valid GUID (not all zeros)
            var is_valid = false;
            for (component.guid) |byte| {
                if (byte != 0) {
                    is_valid = true;
                    break;
                }
            }
            
            if (!is_valid) {
                break;
            }
            
            try components.append(self.allocator, component);
            current_rva += @sizeOf(VBExternalComponent);
        }
        
        return components;
    }
    
    /// Get runtime function imports
    pub fn getRuntimeImports(self: *RuntimeParser) !std.ArrayList(structures.ImportInfo) {
        var imports = std.ArrayList(structures.ImportInfo).empty;
        
        // For now, just return a list of well-known VB runtime functions
        // TODO: Implement proper import directory parsing
        
        const known_functions = [_][]const u8{
            "ThunRTMain",
            "__vbaNew",
            "__vbaFreeObj",
            "__vbaFreeStr",
            "__vbaVarMove",
            "rtcMsgBox",
        };
        
        for (known_functions) |func_name| {
            const import_info = structures.ImportInfo{
                .dll_name = try self.allocator.dupe(u8, structures.VB6_RUNTIME_DLL),
                .function_name = try self.allocator.dupe(u8, func_name),
                .ordinal = null,
                .address = 0,
            };
            try imports.append(self.allocator, import_info);
        }
        
        return imports;
    }
};
