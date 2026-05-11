// VB6 binary detector
const std = @import("std");
const pe = @import("../pe/parser.zig");
const headers = @import("../pe/headers.zig");
const vb_structures = @import("structures.zig");

pub const DetectionResult = struct {
    is_vb: bool,
    is_vb5: bool,
    is_vb6: bool,
    compilation_type: vb_structures.CompilationType,
    binary_type: vb_structures.BinaryType,
    has_forms: bool,
    runtime_dll: ?[]const u8,

    pub fn init() DetectionResult {
        return DetectionResult{
            .is_vb = false,
            .is_vb5 = false,
            .is_vb6 = false,
            .compilation_type = .unknown,
            .binary_type = .unknown,
            .has_forms = false,
            .runtime_dll = null,
        };
    }
};

pub fn detectVB6(pe_file: *const pe.PEFile) DetectionResult {
    var result = DetectionResult.init();

    // Determine binary type
    if (pe_file.isDLL()) {
        // Check if it's an OCX by looking for DllRegisterServer export
        result.binary_type = if (hasOCXExports(pe_file)) .ocx else .dll;
    } else if (pe_file.isExecutable()) {
        result.binary_type = .exe;
    }

    // Check for VB runtime DLL imports
    const runtime_info = checkVBRuntimeImports(pe_file);
    if (runtime_info.found) {
        result.is_vb = true;
        result.is_vb5 = runtime_info.is_vb5;
        result.is_vb6 = runtime_info.is_vb6;
        result.runtime_dll = runtime_info.dll_name;
    } else {
        return result;
    }

    // Detect compilation type (Native vs P-Code)
    result.compilation_type = detectCompilationType(pe_file);

    // Check for forms
    result.has_forms = detectForms(pe_file);

    return result;
}

const RuntimeCheckResult = struct {
    found: bool,
    is_vb5: bool,
    is_vb6: bool,
    dll_name: ?[]const u8,
};

fn checkVBRuntimeImports(pe_file: *const pe.PEFile) RuntimeCheckResult {
    var result = RuntimeCheckResult{
        .found = false,
        .is_vb5 = false,
        .is_vb6 = false,
        .dll_name = null,
    };

    const import_dir = pe_file.getDataDirectory(.import_table) orelse return result;
    if (import_dir.virtual_address == 0) return result;

    var offset: u32 = 0;
    while (true) {
        const entry_rva = import_dir.virtual_address + offset;
        const entry_data = pe_file.rvaToData(entry_rva, headers.ImportDirectoryEntry.SIZE) orelse break;
        const entry = std.mem.bytesToValue(headers.ImportDirectoryEntry, entry_data[0..headers.ImportDirectoryEntry.SIZE]);

        // Null entry marks end of import table
        if (entry.name_rva == 0) break;

        const dll_name = pe_file.readCString(entry.name_rva) orelse {
            offset += headers.ImportDirectoryEntry.SIZE;
            continue;
        };

        // Check for VB runtime DLLs (case-insensitive)
        const dll_name_upper = std.ascii.allocUpperString(pe_file.allocator, dll_name) catch {
            offset += headers.ImportDirectoryEntry.SIZE;
            continue;
        };
        defer pe_file.allocator.free(dll_name_upper);

        if (std.mem.eql(u8, dll_name_upper, vb_structures.VB6_RUNTIME_DLL)) {
            result.found = true;
            result.is_vb6 = true;
            result.dll_name = vb_structures.VB6_RUNTIME_DLL;
            break;
        } else if (std.mem.eql(u8, dll_name_upper, vb_structures.VB5_RUNTIME_DLL)) {
            result.found = true;
            result.is_vb5 = true;
            result.dll_name = vb_structures.VB5_RUNTIME_DLL;
            break;
        }

        offset += headers.ImportDirectoryEntry.SIZE;
    }

    return result;
}

fn detectCompilationType(pe_file: *const pe.PEFile) vb_structures.CompilationType {
    // Heuristic: P-Code binaries typically have smaller .text sections
    // and contain specific P-Code interpreter patterns
    
    const text_section = pe_file.findSection(".text") orelse return .unknown;
    
    // Look for P-Code interpreter signatures
    // P-Code binaries call into MSVBVM60 frequently for instruction interpretation
    // Native binaries have direct x86 code
    
    // For now, use a simple heuristic based on code section size
    // More sophisticated detection would analyze the code patterns
    const code_size = text_section.size_of_raw_data;
    
    // If code section is very small, likely P-Code
    // This is a rough heuristic and should be improved
    if (code_size < 10000) {
        return .pcode;
    }
    
    // Check for P-Code specific patterns in the code section
    if (hasPCodePatterns(pe_file, text_section)) {
        return .pcode;
    }
    
    return .native;
}

fn hasPCodePatterns(pe_file: *const pe.PEFile, text_section: *const headers.SectionHeader) bool {
    // Look for patterns specific to P-Code interpretation
    // P-Code executables have a small stub that calls into the VB runtime
    
    const code_data = pe_file.rvaToData(
        text_section.virtual_address,
        @min(text_section.size_of_raw_data, 1024),
    ) orelse return false;
    
    // Look for frequent calls to VB runtime functions
    // This is a simplified check - real implementation would be more sophisticated
    var call_count: u32 = 0;
    var i: usize = 0;
    while (i + 5 <= code_data.len) : (i += 1) {
        // Check for CALL instruction (0xE8)
        if (code_data[i] == 0xE8) {
            call_count += 1;
            i += 4; // Skip the 4-byte relative address
        }
    }
    
    // P-Code has very few direct calls in the entry stub
    return call_count < 5;
}

fn detectForms(pe_file: *const pe.PEFile) bool {
    // Forms are typically stored in the resource section
    const resource_dir = pe_file.getDataDirectory(.resource_table) orelse return false;
    if (resource_dir.virtual_address == 0) return false;
    
    // VB forms might have specific resource types or patterns
    // This is a placeholder - full implementation would parse resource directory
    return resource_dir.size > 0;
}

fn hasOCXExports(pe_file: *const pe.PEFile) bool {
    const export_dir = pe_file.getDataDirectory(.export_table) orelse return false;
    if (export_dir.virtual_address == 0) return false;
    
    const export_data = pe_file.rvaToData(export_dir.virtual_address, headers.ExportDirectory.SIZE) orelse return false;
    const export_directory = std.mem.bytesToValue(headers.ExportDirectory, export_data[0..headers.ExportDirectory.SIZE]);
    
    // Check for OCX-specific exports
    const names_array_rva = export_directory.address_of_names;
    
    var i: u32 = 0;
    while (i < export_directory.number_of_names) : (i += 1) {
        const name_rva_offset = names_array_rva + i * 4;
        const name_rva_data = pe_file.rvaToData(name_rva_offset, 4) orelse continue;
        const name_rva = std.mem.readInt(u32, name_rva_data[0..4], .little);
        
        const name = pe_file.readCString(name_rva) orelse continue;
        
        // OCX files typically export DllRegisterServer and DllUnregisterServer
        if (std.mem.eql(u8, name, "DllRegisterServer") or
            std.mem.eql(u8, name, "DllUnregisterServer"))
        {
            return true;
        }
    }
    
    return false;
}

pub fn printDetectionResult(result: *const DetectionResult, writer: anytype) !void {
    try writer.print("VB Detection Results:\n", .{});
    try writer.print("  Is VB: {}\n", .{result.is_vb});
    
    if (result.is_vb) {
        try writer.print("  Version: {s}\n", .{
            if (result.is_vb6) "VB6" else if (result.is_vb5) "VB5" else "Unknown",
        });
        try writer.print("  Runtime DLL: {s}\n", .{result.runtime_dll orelse "Unknown"});
        try writer.print("  Binary Type: {s}\n", .{@tagName(result.binary_type)});
        try writer.print("  Compilation: {s}\n", .{@tagName(result.compilation_type)});
        try writer.print("  Has Forms: {}\n", .{result.has_forms});
    }
}

test "detection result init" {
    const result = DetectionResult.init();
    try std.testing.expect(!result.is_vb);
    try std.testing.expect(!result.is_vb5);
    try std.testing.expect(!result.is_vb6);
}
