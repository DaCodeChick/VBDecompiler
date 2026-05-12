// VB6 P-code Detection and Parsing
// Detects if a VB6 binary uses P-code and locates P-code segments

const std = @import("std");
const pe = @import("../pe/parser.zig");
const detector = @import("detector.zig");

/// P-code segment information
pub const PCodeSegment = struct {
    rva: u32,
    size: u32,
    data: []const u8,
    is_startup: bool,     // Is this the startup code segment?
    
    pub fn deinit(self: *PCodeSegment, allocator: std.mem.Allocator) void {
        _ = allocator;
        _ = self;
        // Data is a slice into PE file, not owned
    }
};

/// P-code function entry
pub const PCodeFunction = struct {
    name: []const u8,
    offset: u32,          // Offset within P-code segment
    size: u32,
    
    pub fn deinit(self: *PCodeFunction, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
    }
};

/// P-code parser for VB6 binaries
pub const PCodeParser = struct {
    allocator: std.mem.Allocator,
    pe_file: *const pe.PEFile,
    
    pub fn init(allocator: std.mem.Allocator, pe_file: *const pe.PEFile) PCodeParser {
        return .{
            .allocator = allocator,
            .pe_file = pe_file,
        };
    }
    
    /// Detect if binary uses P-code
    pub fn isPCode(self: *PCodeParser) bool {
        // Check for VB6 first
        const vb_info = detector.detectVB6(self.pe_file);
        if (!vb_info.is_vb6) return false;
        
        // P-code binaries have specific characteristics:
        // 1. Presence of MSVBVM60.DLL import (runtime interpreter)
        // 2. Smaller .text section (less native code)
        // 3. Presence of specific P-code section or resource
        
        // Check for MSVBVM60.DLL reference
        if (!self.hasPCodeRuntime()) return false;
        
        // Check for P-code characteristics in binary
        if (self.findPCodeSegment()) |_| {
            return true;
        }
        
        return false;
    }
    
    /// Check if binary imports P-code runtime
    fn hasPCodeRuntime(self: *PCodeParser) bool {
        const runtime_dlls = [_][]const u8{
            "MSVBVM60.DLL",
            "MSVBVM50.DLL",
        };
        
        for (runtime_dlls) |dll| {
            if (std.mem.indexOf(u8, self.pe_file.data, dll) != null) {
                return true;
            }
        }
        
        return false;
    }
    
    /// Find P-code segment in binary
    pub fn findPCodeSegment(self: *PCodeParser) ?PCodeSegment {
        // P-code is typically stored in a specific section or resource
        // Common locations:
        // 1. .vbp section (VB P-code)
        // 2. Resource named "PCODE" or similar
        // 3. Embedded in .data section after object table
        
        // Try to find .vbp section
        for (self.pe_file.sections) |*section| {
            const name = section.getName();
            if (std.mem.eql(u8, name, ".vbp")) {
                const data = self.pe_file.rvaToData(
                    section.virtual_address,
                    section.size_of_raw_data,
                ) orelse continue;
                
                return PCodeSegment{
                    .rva = section.virtual_address,
                    .size = section.size_of_raw_data,
                    .data = data,
                    .is_startup = false,
                };
            }
        }
        
        // Try .data section with heuristics
        for (self.pe_file.sections) |*section| {
            const name = section.getName();
            if (std.mem.eql(u8, name, ".data")) {
                const data = self.pe_file.rvaToData(
                    section.virtual_address,
                    section.size_of_raw_data,
                ) orelse continue;
                
                // Look for P-code signature or patterns
                if (self.looksLikePCode(data)) {
                    return PCodeSegment{
                        .rva = section.virtual_address,
                        .size = section.size_of_raw_data,
                        .data = data,
                        .is_startup = false,
                    };
                }
            }
        }
        
        return null;
    }
    
    /// Heuristic check if data looks like P-code
    fn looksLikePCode(self: *PCodeParser, data: []const u8) bool {
        _ = self;
        if (data.len < 16) return false;
        
        // P-code typically has:
        // - High density of valid opcodes (0x01-0x90 range mostly)
        // - Structured patterns (push/op/pop sequences)
        // - Return instructions (0x44) at function boundaries
        
        var valid_opcodes: usize = 0;
        var total_checked: usize = 0;
        const sample_size = @min(data.len, 256);
        
        var i: usize = 0;
        while (i < sample_size) : (i += 1) {
            const byte = data[i];
            // Valid opcode range (loose check)
            if (byte >= 0x01 and byte <= 0x90) {
                valid_opcodes += 1;
            }
            total_checked += 1;
        }
        
        // If more than 60% look like valid opcodes, probably P-code
        const ratio = (@as(f32, @floatFromInt(valid_opcodes)) / @as(f32, @floatFromInt(total_checked)));
        return ratio > 0.6;
    }
    
    /// Extract all P-code segments
    pub fn extractSegments(self: *PCodeParser) !std.ArrayList(PCodeSegment) {
        var segments: std.ArrayList(PCodeSegment) = .empty;
        
        if (self.findPCodeSegment()) |segment| {
            try segments.append(self.allocator, segment);
        }
        
        return segments;
    }
    
    /// Parse P-code function table
    /// VB6 P-code binaries have a function table pointing to P-code routines
    pub fn parseFunctionTable(self: *PCodeParser, segment: *const PCodeSegment) !std.ArrayList(PCodeFunction) {
        var functions: std.ArrayList(PCodeFunction) = .empty;
        
        // Function table format (simplified):
        // At the start of P-code segment, there's typically a table:
        // struct FunctionEntry {
        //     u32 offset;      // Offset to function in segment
        //     u32 size;        // Size of function
        //     u32 name_offset; // Offset to function name
        // }
        
        const data = segment.data;
        if (data.len < 12) return functions;
        
        // Read function count (first 4 bytes)
        const func_count = std.mem.readInt(u32, data[0..4], .little);
        
        // Sanity check
        if (func_count > 1000 or func_count == 0) {
            // Doesn't look like a valid function table
            return functions;
        }
        
        var offset: usize = 4;
        var i: u32 = 0;
        while (i < func_count and offset + 12 <= data.len) : (i += 1) {
            const func_offset = std.mem.readInt(u32, data[offset..][0..4], .little);
            const func_size = std.mem.readInt(u32, data[offset + 4..][0..4], .little);
            const name_offset = std.mem.readInt(u32, data[offset + 8..][0..4], .little);
            
            offset += 12;
            
            // Read function name if offset is valid
            var name: []const u8 = "unknown";
            if (name_offset < data.len) {
                // Find null terminator
                var name_end = name_offset;
                while (name_end < data.len and data[name_end] != 0) : (name_end += 1) {}
                
                if (name_end > name_offset) {
                    name = try self.allocator.dupe(u8, data[name_offset..name_end]);
                }
            }
            
            const function = PCodeFunction{
                .name = if (std.mem.eql(u8, name, "unknown"))
                    try std.fmt.allocPrint(self.allocator, "sub_{X:0>8}", .{func_offset})
                else
                    name,
                .offset = func_offset,
                .size = func_size,
            };
            
            try functions.append(self.allocator, function);
        }
        
        return functions;
    }
};
