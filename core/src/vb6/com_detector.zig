// VB6 COM Interface Detection
// Detects and parses COM interfaces, GUIDs, and type libraries

const std = @import("std");
const pe = @import("../pe/parser.zig");

/// GUID structure (128-bit)
/// Note: Not using packed struct since arrays aren't supported in packed structs in Zig 0.16
pub const GUID = extern struct {
    data1: u32,
    data2: u16,
    data3: u16,
    data4: [8]u8,
    
    /// Format GUID as string {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
    pub fn format(self: *const GUID, allocator: std.mem.Allocator) ![]const u8 {
        return try std.fmt.allocPrint(allocator, "{{{X:0>8}-{X:0>4}-{X:0>4}-{X:0>2}{X:0>2}-{X:0>2}{X:0>2}{X:0>2}{X:0>2}{X:0>2}{X:0>2}}}", .{
            self.data1,
            self.data2,
            self.data3,
            self.data4[0],
            self.data4[1],
            self.data4[2],
            self.data4[3],
            self.data4[4],
            self.data4[5],
            self.data4[6],
            self.data4[7],
        });
    }
    
    /// Check if GUID is null
    pub fn isNull(self: *const GUID) bool {
        return self.data1 == 0 and self.data2 == 0 and self.data3 == 0 and
               self.data4[0] == 0 and self.data4[1] == 0 and
               self.data4[2] == 0 and self.data4[3] == 0 and
               self.data4[4] == 0 and self.data4[5] == 0 and
               self.data4[6] == 0 and self.data4[7] == 0;
    }
};

/// Well-known COM GUIDs
pub const WellKnownGUIDs = struct {
    pub const IUnknown = GUID{
        .data1 = 0x00000000,
        .data2 = 0x0000,
        .data3 = 0x0000,
        .data4 = [8]u8{ 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 },
    };
    
    pub const IDispatch = GUID{
        .data1 = 0x00020400,
        .data2 = 0x0000,
        .data3 = 0x0000,
        .data4 = [8]u8{ 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 },
    };
    
    pub const IConnectionPointContainer = GUID{
        .data1 = 0xB196B284,
        .data2 = 0xBAB4,
        .data3 = 0x101A,
        .data4 = [8]u8{ 0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1D, 0x07 },
    };
};

/// COM interface information
pub const COMInterface = struct {
    guid: GUID,
    name: []const u8,
    vtable_offset: u32,
    method_count: u32,
    
    pub fn deinit(self: *COMInterface, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
    }
};

/// COM detector
pub const COMDetector = struct {
    allocator: std.mem.Allocator,
    pe_file: *const pe.PEFile,
    
    pub fn init(allocator: std.mem.Allocator, pe_file: *const pe.PEFile) COMDetector {
        return COMDetector{
            .allocator = allocator,
            .pe_file = pe_file,
        };
    }
    
    /// Helper to get section by name
    fn getSectionByName(self: *const COMDetector, name: []const u8) ?*const pe.headers.SectionHeader {
        for (self.pe_file.sections) |*section| {
            const section_name = section.getName();
            if (std.mem.eql(u8, section_name, name)) {
                return section;
            }
        }
        return null;
    }
    
    /// Find all GUIDs in the binary
    pub fn findGUIDs(self: *COMDetector) !std.ArrayList(GUIDLocation) {
        var guids = std.ArrayList(GUIDLocation).empty;
        
        // Search in .data and .rdata sections
        const sections = [_][]const u8{ ".data", ".rdata", ".text" };
        
        for (sections) |section_name| {
            const section = self.getSectionByName(section_name) orelse continue;
            const section_rva = section.virtual_address;
            const section_size = section.size_of_raw_data;
            const data = self.pe_file.rvaToData(section_rva, section_size) orelse continue;
            
            var offset: usize = 0;
            while (offset + @sizeOf(GUID) <= data.len) : (offset += 4) {
                var guid: GUID = undefined;
                @memcpy(std.mem.asBytes(&guid), data[offset..][0..@sizeOf(GUID)]);
                
                // Heuristic: valid GUID should not be all zeros or all FFs
                if (!guid.isNull() and !self.isInvalidGUID(&guid)) {
                    const rva = section_rva + @as(u32, @intCast(offset));
                    const guid_loc = GUIDLocation{
                        .guid = guid,
                        .rva = rva,
                        .section = try self.allocator.dupe(u8, section_name),
                    };
                    try guids.append(self.allocator, guid_loc);
                }
            }
        }
        
        return guids;
    }
    
    /// Check if GUID looks invalid
    fn isInvalidGUID(self: *COMDetector, guid: *const GUID) bool {
        _ = self;
        
        // All 0xFF
        if (guid.data1 == 0xFFFFFFFF and guid.data2 == 0xFFFF and guid.data3 == 0xFFFF) {
            return true;
        }
        
        // Very low values (likely not a GUID)
        if (guid.data1 < 0x100 and guid.data2 == 0 and guid.data3 == 0) {
            return true;
        }
        
        return false;
    }
    
    /// Find IUnknown vtables
    pub fn findVTables(self: *COMDetector) !std.ArrayList(VTableInfo) {
        var vtables = std.ArrayList(VTableInfo).empty;
        
        const data_section = self.getSectionByName(".data") orelse return vtables;
        
        // Get the data for this section using rvaToData
        const section_rva = data_section.virtual_address;
        const section_size = data_section.size_of_raw_data;
        const data = self.pe_file.rvaToData(section_rva, section_size) orelse return vtables;
        
        // VTable is an array of function pointers
        // Typically starts with IUnknown methods: QueryInterface, AddRef, Release
        
        var offset: usize = 0;
        while (offset + 12 <= data.len) : (offset += 4) {
            const ptr1 = std.mem.readInt(u32, data[offset..][0..4], .little);
            const ptr2 = std.mem.readInt(u32, data[offset + 4..][0..4], .little);
            const ptr3 = std.mem.readInt(u32, data[offset + 8..][0..4], .little);
            
            // Check if pointers look valid (in .text section)
            if (self.looksLikeCodePointer(ptr1) and
                self.looksLikeCodePointer(ptr2) and
                self.looksLikeCodePointer(ptr3))
            {
                const rva = data_section.virtual_address + @as(u32, @intCast(offset));
                const vtable = VTableInfo{
                    .rva = rva,
                    .method_count = 3, // At least IUnknown methods
                    .methods = std.ArrayList(u32).empty,
                };
                try vtables.append(self.allocator, vtable);
            }
        }
        
        return vtables;
    }
    
    /// Check if value looks like a code pointer
    fn looksLikeCodePointer(self: *COMDetector, ptr: u32) bool {
        const image_base = self.pe_file.getImageBase();
        
        // Should be above image base
        if (ptr < image_base) {
            return false;
        }
        
        const rva = ptr - image_base;
        
        // Check if RVA is in .text section
        const text_section = self.getSectionByName(".text") orelse return false;
        
        const section_start = text_section.virtual_address;
        const section_end = section_start + text_section.virtual_size;
        
        return rva >= section_start and rva < section_end;
    }
    
    /// Find type library references
    pub fn findTypeLibraries(self: *COMDetector) !std.ArrayList(TypeLibraryRef) {
        var type_libs = std.ArrayList(TypeLibraryRef).empty;
        
        // Type libraries are referenced via registry keys like:
        // "TypeLib\\{GUID}\\Version\\LCID\\Platform"
        
        // Look for GUID patterns followed by version numbers
        var guids = try self.findGUIDs();
        defer {
            for (guids.items) |*guid_loc| {
                guid_loc.deinit(self.allocator);
            }
            guids.deinit(self.allocator);
        }
        
        for (guids.items) |guid_loc| {
            // Check if followed by version-like data
            const data = self.pe_file.rvaToData(guid_loc.rva + @sizeOf(GUID), 8) orelse continue;
            
            if (data.len >= 4) {
                const maybe_version = std.mem.readInt(u32, data[0..4], .little);
                
                // Version numbers are typically < 10.0
                if (maybe_version > 0 and maybe_version < 0x0A00) {
                    const major = (maybe_version >> 16) & 0xFFFF;
                    const minor = maybe_version & 0xFFFF;
                    
                    const type_lib = TypeLibraryRef{
                        .guid = guid_loc.guid,
                        .version_major = @intCast(major),
                        .version_minor = @intCast(minor),
                        .name = try self.allocator.dupe(u8, "Unknown"),
                    };
                    
                    try type_libs.append(self.allocator, type_lib);
                }
            }
        }
        
        return type_libs;
    }
    
    /// Detect if binary uses COM/ActiveX
    pub fn detectCOMUsage(self: *COMDetector) !COMUsageInfo {
        var info = COMUsageInfo{
            .uses_com = false,
            .has_type_lib = false,
            .implements_interfaces = false,
            .interface_count = 0,
            .guid_count = 0,
        };
        
        // Check imports for COM functions
        const has_ole_imports = try self.hasOLEImports();
        if (has_ole_imports) {
            info.uses_com = true;
        }
        
        // Count GUIDs
        var guids = try self.findGUIDs();
        info.guid_count = guids.items.len;
        
        for (guids.items) |*guid_loc| {
            guid_loc.deinit(self.allocator);
        }
        guids.deinit(self.allocator);
        
        if (info.guid_count > 0) {
            info.uses_com = true;
        }
        
        // Count vtables
        var vtables = try self.findVTables();
        info.interface_count = vtables.items.len;
        info.implements_interfaces = info.interface_count > 0;
        
        for (vtables.items) |*vtable| {
            vtable.deinit(self.allocator);
        }
        vtables.deinit(self.allocator);
        
        // Check for type libraries
        var type_libs = try self.findTypeLibraries();
        info.has_type_lib = type_libs.items.len > 0;
        
        for (type_libs.items) |*tlib| {
            tlib.deinit(self.allocator);
        }
        type_libs.deinit(self.allocator);
        
        return info;
    }
    
    /// Check if binary imports OLE/COM functions
    /// Simplified: scans for known OLE DLL names in the binary
    fn hasOLEImports(self: *COMDetector) !bool {
        const ole_dlls = [_][]const u8{
            "OLEAUT32.DLL",
            "OLE32.DLL",
            "OLEPRO32.DLL",
        };
        
        // Scan binary data for OLE DLL name strings
        for (ole_dlls) |ole_dll| {
            if (std.mem.indexOf(u8, self.pe_file.data, ole_dll) != null) {
                return true;
            }
        }
        
        return false;
    }
};

/// GUID location in binary
pub const GUIDLocation = struct {
    guid: GUID,
    rva: u32,
    section: []const u8,
    
    pub fn deinit(self: *GUIDLocation, allocator: std.mem.Allocator) void {
        allocator.free(self.section);
    }
};

/// VTable information
pub const VTableInfo = struct {
    rva: u32,
    method_count: u32,
    methods: std.ArrayList(u32),
    
    pub fn deinit(self: *VTableInfo, allocator: std.mem.Allocator) void {
        self.methods.deinit(allocator);
    }
};

/// Type library reference
pub const TypeLibraryRef = struct {
    guid: GUID,
    version_major: u16,
    version_minor: u16,
    name: []const u8,
    
    pub fn deinit(self: *TypeLibraryRef, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
    }
};

/// COM usage information
pub const COMUsageInfo = struct {
    uses_com: bool,
    has_type_lib: bool,
    implements_interfaces: bool,
    interface_count: usize,
    guid_count: usize,
};
