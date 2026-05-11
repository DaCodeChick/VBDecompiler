// PE file parser
const std = @import("std");
const headers = @import("headers.zig");

pub const PEError = error{
    InvalidDOSSignature,
    InvalidPESignature,
    InvalidMachineType,
    InvalidOptionalHeader,
    SectionNotFound,
    InvalidRVA,
    OutOfBounds,
    NotSupported,
};

pub const PEFile = struct {
    allocator: std.mem.Allocator,
    data: []const u8,
    dos_header: headers.DosHeader,
    pe_offset: u32,
    coff_header: headers.CoffHeader,
    optional_header: headers.OptionalHeader32,
    data_directories: []headers.DataDirectory,
    sections: []headers.SectionHeader,

    pub fn init(allocator: std.mem.Allocator, data: []const u8) !PEFile {
        if (data.len < headers.DosHeader.SIZE) {
            return PEError.InvalidDOSSignature;
        }

        // Parse DOS header
        const dos_header = try parseDosHeader(data);
        
        // Validate DOS signature
        if (dos_header.e_magic != headers.DosHeader.MAGIC) {
            return PEError.InvalidDOSSignature;
        }

        // Get PE offset
        const pe_offset: u32 = @intCast(dos_header.e_lfanew);
        if (pe_offset + 4 > data.len) {
            return PEError.InvalidPESignature;
        }

        // Validate PE signature
        const pe_signature = std.mem.readInt(u32, data[pe_offset..][0..4], .little);
        if (pe_signature != headers.PE_SIGNATURE) {
            return PEError.InvalidPESignature;
        }

        // Parse COFF header
        const coff_offset = pe_offset + 4;
        if (coff_offset + headers.CoffHeader.SIZE > data.len) {
            return PEError.InvalidPESignature;
        }
        const coff_header = try parseCoffHeader(data[coff_offset..]);

        // Validate machine type (VB6 is 32-bit x86)
        if (coff_header.machine != headers.CoffHeader.MACHINE_I386) {
            return PEError.InvalidMachineType;
        }

        // Parse optional header
        const opt_header_offset = coff_offset + headers.CoffHeader.SIZE;
        if (opt_header_offset + coff_header.size_of_optional_header > data.len) {
            return PEError.InvalidOptionalHeader;
        }
        const optional_header = try parseOptionalHeader32(data[opt_header_offset..]);

        // Parse data directories
        const data_dir_offset = opt_header_offset + headers.OptionalHeader32.SIZE;
        const num_dirs = @min(optional_header.number_of_rva_and_sizes, headers.OptionalHeader32.NUM_DATA_DIRECTORIES);
        const data_directories = try allocator.alloc(headers.DataDirectory, num_dirs);
        errdefer allocator.free(data_directories);

        for (0..num_dirs) |i| {
            const offset = data_dir_offset + i * headers.DataDirectory.SIZE;
            if (offset + headers.DataDirectory.SIZE > data.len) break;
            data_directories[i] = try parseDataDirectory(data[offset..]);
        }

        // Parse section headers
        const sections_offset = opt_header_offset + coff_header.size_of_optional_header;
        const sections = try allocator.alloc(headers.SectionHeader, coff_header.number_of_sections);
        errdefer allocator.free(sections);

        for (0..coff_header.number_of_sections) |i| {
            const offset = sections_offset + i * headers.SectionHeader.SIZE;
            if (offset + headers.SectionHeader.SIZE > data.len) {
                return PEError.InvalidPESignature;
            }
            sections[i] = try parseSectionHeader(data[offset..]);
        }

        return PEFile{
            .allocator = allocator,
            .data = data,
            .dos_header = dos_header,
            .pe_offset = pe_offset,
            .coff_header = coff_header,
            .optional_header = optional_header,
            .data_directories = data_directories,
            .sections = sections,
        };
    }

    pub fn deinit(self: *PEFile) void {
        self.allocator.free(self.data_directories);
        self.allocator.free(self.sections);
    }

    pub fn isDLL(self: *const PEFile) bool {
        return (self.coff_header.characteristics & headers.CoffHeader.IMAGE_FILE_DLL) != 0;
    }

    pub fn isExecutable(self: *const PEFile) bool {
        return (self.coff_header.characteristics & headers.CoffHeader.IMAGE_FILE_EXECUTABLE_IMAGE) != 0;
    }

    pub fn getEntryPoint(self: *const PEFile) u32 {
        return self.optional_header.address_of_entry_point;
    }

    pub fn getImageBase(self: *const PEFile) u32 {
        return self.optional_header.image_base;
    }

    pub fn findSection(self: *const PEFile, name: []const u8) ?*const headers.SectionHeader {
        for (self.sections) |*section| {
            const section_name = section.getName();
            if (std.mem.eql(u8, section_name, name)) {
                return section;
            }
        }
        return null;
    }

    pub fn rvaToOffset(self: *const PEFile, rva: u32) ?u32 {
        return headers.rvaToOffset(rva, self.sections);
    }

    pub fn rvaToData(self: *const PEFile, rva: u32, size: usize) ?[]const u8 {
        const offset = self.rvaToOffset(rva) orelse return null;
        if (offset + size > self.data.len) return null;
        return self.data[offset..][0..size];
    }

    pub fn getDataDirectory(self: *const PEFile, index: headers.DataDirectoryIndex) ?headers.DataDirectory {
        const idx = @intFromEnum(index);
        if (idx >= self.data_directories.len) return null;
        const dir = self.data_directories[idx];
        if (dir.virtual_address == 0 or dir.size == 0) return null;
        return dir;
    }

    pub fn readCString(self: *const PEFile, rva: u32) ?[]const u8 {
        const offset = self.rvaToOffset(rva) orelse return null;
        if (offset >= self.data.len) return null;
        
        const start = offset;
        var end = offset;
        while (end < self.data.len and self.data[end] != 0) {
            end += 1;
        }
        
        return self.data[start..end];
    }
};

fn parseDosHeader(data: []const u8) !headers.DosHeader {
    if (data.len < headers.DosHeader.SIZE) {
        return PEError.InvalidDOSSignature;
    }
    return std.mem.bytesToValue(headers.DosHeader, data[0..headers.DosHeader.SIZE]);
}

fn parseCoffHeader(data: []const u8) !headers.CoffHeader {
    if (data.len < headers.CoffHeader.SIZE) {
        return PEError.InvalidPESignature;
    }
    return std.mem.bytesToValue(headers.CoffHeader, data[0..headers.CoffHeader.SIZE]);
}

fn parseOptionalHeader32(data: []const u8) !headers.OptionalHeader32 {
    if (data.len < headers.OptionalHeader32.SIZE) {
        return PEError.InvalidOptionalHeader;
    }
    const header = std.mem.bytesToValue(headers.OptionalHeader32, data[0..headers.OptionalHeader32.SIZE]);
    if (header.magic != headers.OptionalHeader32.MAGIC_PE32) {
        return PEError.NotSupported; // 64-bit not supported for VB6
    }
    return header;
}

fn parseDataDirectory(data: []const u8) !headers.DataDirectory {
    if (data.len < headers.DataDirectory.SIZE) {
        return PEError.InvalidOptionalHeader;
    }
    return std.mem.bytesToValue(headers.DataDirectory, data[0..headers.DataDirectory.SIZE]);
}

fn parseSectionHeader(data: []const u8) !headers.SectionHeader {
    if (data.len < headers.SectionHeader.SIZE) {
        return PEError.InvalidPESignature;
    }
    return std.mem.bytesToValue(headers.SectionHeader, data[0..headers.SectionHeader.SIZE]);
}

pub fn parseFromFile(allocator: std.mem.Allocator, path: []const u8) !PEFile {
    // Use Zig stdlib for file reading
    var threaded = std.Io.Threaded.init(allocator, .{});
    defer threaded.deinit();
    const io = threaded.io();
    
    const file = try std.Io.Dir.openFile(std.Io.Dir.cwd(), io, path, .{});
    defer file.close(io);
    
    // Get file size
    const stat = try file.stat(io);
    const file_size = stat.size;
    
    // Read file
    const data = try allocator.alloc(u8, file_size);
    errdefer allocator.free(data);
    
    // Read entire file from beginning
    var slice = [_][]u8{data};
    const bytes_read = try file.readPositional(io, &slice, 0);
    if (bytes_read != file_size) return error.UnexpectedEof;

    return try PEFile.init(allocator, data);
}

test "PE parser basic" {
    const allocator = std.testing.allocator;
    
    // Create a minimal valid DOS header
    var dos_data: [64]u8 = [_]u8{0} ** 64;
    dos_data[0] = 'M';
    dos_data[1] = 'Z';
    std.mem.writeInt(i32, dos_data[60..64], 64, .little); // e_lfanew
    
    // This should fail with invalid PE signature (as we didn't add full PE)
    const result = PEFile.init(allocator, &dos_data);
    try std.testing.expectError(PEError.InvalidPESignature, result);
}
