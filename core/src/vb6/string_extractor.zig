// VB6 String Table Extractor
// Extracts and parses VB6 string resources and BSTR strings

const std = @import("std");
const pe = @import("../pe/parser.zig");
const structures = @import("structures.zig");

/// String extractor
pub const StringExtractor = struct {
    allocator: std.mem.Allocator,
    pe_file: *const pe.PEFile,
    
    pub fn init(allocator: std.mem.Allocator, pe_file: *const pe.PEFile) StringExtractor {
        return StringExtractor{
            .allocator = allocator,
            .pe_file = pe_file,
        };
    }
    
    /// Helper to get section by name
    fn getSectionByName(self: *const StringExtractor, name: []const u8) ?*const pe.headers.SectionHeader {
        for (self.pe_file.sections) |*section| {
            const section_name = section.getName();
            if (std.mem.eql(u8, section_name, name)) {
                return section;
            }
        }
        return null;
    }
    
    /// Extract all strings from binary
    pub fn extractAll(self: *StringExtractor) !std.ArrayList(structures.StringResource) {
        var strings = std.ArrayList(structures.StringResource).empty;
        
        // Extract from different sources
        const resource_strings = try self.extractFromResources();
        const data_strings = try self.extractFromDataSection();
        const rdata_strings = try self.extractFromRDataSection();
        
        for (resource_strings.items) |str| {
            try strings.append(self.allocator, str);
        }
        
        for (data_strings.items) |str| {
            try strings.append(self.allocator, str);
        }
        
        for (rdata_strings.items) |str| {
            try strings.append(self.allocator, str);
        }
        
        return strings;
    }
    
    /// Extract strings from PE resources
    fn extractFromResources(_: *StringExtractor) !std.ArrayList(structures.StringResource) {
        const strings = std.ArrayList(structures.StringResource).empty;
        
        // RT_STRING resources are typically organized in blocks of 16 strings
        // Each block has a WORD count followed by WCHAR strings
        
        // TODO: Implement resource parsing
        // This requires walking the resource directory tree
        
        return strings;
    }
    
    /// Extract strings from .data section
    fn extractFromDataSection(self: *StringExtractor) !std.ArrayList(structures.StringResource) {
        var strings = std.ArrayList(structures.StringResource).empty;
        
        const data_section = self.getSectionByName(".data") orelse return strings;
        const section_rva = data_section.virtual_address;
        const section_size = data_section.size_of_raw_data;
        const data = self.pe_file.rvaToData(section_rva, section_size) orelse return strings;
        
        // Look for BSTR-style strings (length-prefixed unicode)
        var offset: usize = 0;
        while (offset + 4 < data.len) {
            const str = try self.parseBSTR(data[offset..], section_rva + @as(u32, @intCast(offset)));
            if (str) |s| {
                try strings.append(self.allocator, s);
                offset += 4 + s.value.len; // Skip length + string data
            } else {
                offset += 1;
            }
        }
        
        return strings;
    }
    
    /// Extract strings from .rdata section
    fn extractFromRDataSection(self: *StringExtractor) !std.ArrayList(structures.StringResource) {
        var strings = std.ArrayList(structures.StringResource).empty;
        
        const rdata_section = self.getSectionByName(".rdata") orelse return strings;
        const section_rva = rdata_section.virtual_address;
        const section_size = rdata_section.size_of_raw_data;
        const data = self.pe_file.rvaToData(section_rva, section_size) orelse return strings;
        
        // Scan for null-terminated ASCII strings
        var offset: usize = 0;
        while (offset < data.len) {
            const str = try self.parseAsciiString(data[offset..], section_rva + @as(u32, @intCast(offset)));
            if (str) |s| {
                if (s.value.len >= 4) { // Only keep strings >= 4 chars
                    try strings.append(self.allocator, s);
                } else {
                    self.allocator.free(s.value);
                }
                offset += s.value.len + 1; // Skip string + null
            } else {
                offset += 1;
            }
        }
        
        return strings;
    }
    
    /// Parse BSTR (Basic String) format
    /// Format: [4-byte length][Unicode string][2-byte null]
    fn parseBSTR(self: *StringExtractor, data: []const u8, rva: u32) !?structures.StringResource {
        if (data.len < 6) { // Minimum: length + null terminator
            return null;
        }
        
        const length = std.mem.readInt(u32, data[0..4], .little);
        
        // Sanity check: length should be reasonable (< 64KB)
        if (length == 0 or length > 65536 or length % 2 != 0) {
            return null;
        }
        
        if (data.len < 4 + length + 2) {
            return null;
        }
        
        // Check if it's valid Unicode (no embedded nulls except terminator)
        var i: usize = 4;
        var char_count: usize = 0;
        while (i < 4 + length) : (i += 2) {
            const wchar = std.mem.readInt(u16, data[i..][0..2], .little);
            if (wchar == 0) {
                return null; // Embedded null - not a valid BSTR
            }
            if (wchar >= 32 and wchar <= 126) { // ASCII printable range
                char_count += 1;
            } else if (wchar > 127) { // Unicode
                char_count += 1;
            }
        }
        
        if (char_count < 2) {
            return null; // Too short or not readable
        }
        
        // Convert Unicode to UTF-8
        const unicode_data = data[4..][0..length];
        const utf8_str = try self.unicodeToUtf8(unicode_data);
        
        return structures.StringResource{
            .id = 0,
            .rva = rva,
            .value = utf8_str,
        };
    }
    
    /// Parse ASCII null-terminated string
    fn parseAsciiString(self: *StringExtractor, data: []const u8, rva: u32) !?structures.StringResource {
        if (data.len == 0) {
            return null;
        }
        
        // Check first character is printable
        if (data[0] < 32 or data[0] > 126) {
            return null;
        }
        
        // Find null terminator
        var length: usize = 0;
        var printable_count: usize = 0;
        while (length < data.len and length < 1024) : (length += 1) {
            if (data[length] == 0) {
                break;
            }
            if (data[length] >= 32 and data[length] <= 126) {
                printable_count += 1;
            } else if (data[length] != 9 and data[length] != 10 and data[length] != 13) {
                // Not a tab, newline, or carriage return
                return null;
            }
        }
        
        if (length == 0 or printable_count < 2) {
            return null;
        }
        
        // Must be mostly printable
        if (printable_count * 10 < length * 7) { // 70% printable
            return null;
        }
        
        const str = try self.allocator.dupe(u8, data[0..length]);
        
        return structures.StringResource{
            .id = 0,
            .rva = rva,
            .value = str,
        };
    }
    
    /// Convert UTF-16LE to UTF-8
    fn unicodeToUtf8(self: *StringExtractor, unicode_data: []const u8) ![]const u8 {
        const char_count = unicode_data.len / 2;
        
        // Worst case: each UTF-16 char becomes 3 UTF-8 bytes
        var utf8_buffer = try self.allocator.alloc(u8, char_count * 3);
        var utf8_len: usize = 0;
        
        var i: usize = 0;
        while (i + 1 < unicode_data.len) : (i += 2) {
            const wchar = std.mem.readInt(u16, unicode_data[i..][0..2], .little);
            
            if (wchar <= 0x7F) {
                // 1-byte UTF-8
                utf8_buffer[utf8_len] = @intCast(wchar);
                utf8_len += 1;
            } else if (wchar <= 0x7FF) {
                // 2-byte UTF-8
                utf8_buffer[utf8_len] = @intCast(0xC0 | (wchar >> 6));
                utf8_buffer[utf8_len + 1] = @intCast(0x80 | (wchar & 0x3F));
                utf8_len += 2;
            } else {
                // 3-byte UTF-8
                utf8_buffer[utf8_len] = @intCast(0xE0 | (wchar >> 12));
                utf8_buffer[utf8_len + 1] = @intCast(0x80 | ((wchar >> 6) & 0x3F));
                utf8_buffer[utf8_len + 2] = @intCast(0x80 | (wchar & 0x3F));
                utf8_len += 3;
            }
        }
        
        // Trim to actual size
        const result = try self.allocator.dupe(u8, utf8_buffer[0..utf8_len]);
        self.allocator.free(utf8_buffer);
        
        return result;
    }
    
    /// Find strings by pattern
    pub fn findPattern(self: *StringExtractor, pattern: []const u8) !std.ArrayList(structures.StringResource) {
        const all_strings = try self.extractAll();
        defer {
            for (all_strings.items) |*str| {
                str.deinit(self.allocator);
            }
            all_strings.deinit(self.allocator);
        }
        
        var matches = std.ArrayList(structures.StringResource).empty;
        
        for (all_strings.items) |str| {
            if (std.mem.indexOf(u8, str.value, pattern) != null) {
                const matched = structures.StringResource{
                    .id = str.id,
                    .rva = str.rva,
                    .value = try self.allocator.dupe(u8, str.value),
                };
                try matches.append(self.allocator, matched);
            }
        }
        
        return matches;
    }
    
    /// Get string at specific RVA
    pub fn getStringAtRVA(self: *StringExtractor, rva: u32) !?[]const u8 {
        const data = self.pe_file.rvaToData(rva, 1024) orelse return null;
        
        // Try BSTR first
        if (data.len >= 6) {
            const bstr = try self.parseBSTR(data, rva);
            if (bstr) |str| {
                return str.value;
            }
        }
        
        // Try ASCII string
        const ascii_str = try self.parseAsciiString(data, rva);
        if (ascii_str) |str| {
            return str.value;
        }
        
        return null;
    }
};

/// String statistics
pub const StringStatistics = struct {
    total_count: usize,
    unique_count: usize,
    total_bytes: usize,
    ascii_count: usize,
    unicode_count: usize,
    avg_length: f32,
};

/// Calculate string statistics
pub fn calculateStatistics(allocator: std.mem.Allocator, strings: []const structures.StringResource) !StringStatistics {
    var unique_set = std.StringHashMap(void).init(allocator);
    defer unique_set.deinit();
    
    var total_bytes: usize = 0;
    var ascii_count: usize = 0;
    var unicode_count: usize = 0;
    
    for (strings) |str| {
        total_bytes += str.value.len;
        
        // Check if ASCII or Unicode
        var is_ascii = true;
        for (str.value) |byte| {
            if (byte > 127) {
                is_ascii = false;
                break;
            }
        }
        
        if (is_ascii) {
            ascii_count += 1;
        } else {
            unicode_count += 1;
        }
        
        try unique_set.put(str.value, {});
    }
    
    const avg_length = if (strings.len > 0) @as(f32, @floatFromInt(total_bytes)) / @as(f32, @floatFromInt(strings.len)) else 0.0;
    
    return StringStatistics{
        .total_count = strings.len,
        .unique_count = unique_set.count(),
        .total_bytes = total_bytes,
        .ascii_count = ascii_count,
        .unicode_count = unicode_count,
        .avg_length = avg_length,
    };
}
