// VB Decompiler CLI tool
const std = @import("std");
const pe = @import("pe/parser.zig");
const vb6 = @import("vb6/detector.zig");
const disasm = @import("disasm/disassembler.zig");
const Instruction = @import("disasm/instruction.zig").Instruction;

const c = @cImport({
    @cInclude("stdio.h");
});

const Command = enum {
    analyze,
    sections,
    disasm,
    help,
};

fn printUsage() void {
    _ = c.printf("VBDecompiler CLI v0.1.0\n");
    _ = c.printf("Usage: vbdecomp <command> [options] <file>\n\n");
    _ = c.printf("Commands:\n");
    _ = c.printf("  analyze <file>              - Analyze VB6 binary\n");
    _ = c.printf("  sections <file>             - List PE sections\n");
    _ = c.printf("  disasm <file> <address>     - Disassemble at address (hex)\n");
    _ = c.printf("  help                        - Show this help\n\n");
    _ = c.printf("Examples:\n");
    _ = c.printf("  vbdecomp analyze app.exe\n");
    _ = c.printf("  vbdecomp sections app.exe\n");
    _ = c.printf("  vbdecomp disasm app.exe 0x401000\n");
}

fn parseCommand(arg: []const u8) ?Command {
    if (std.mem.eql(u8, arg, "analyze")) return .analyze;
    if (std.mem.eql(u8, arg, "sections")) return .sections;
    if (std.mem.eql(u8, arg, "disasm")) return .disasm;
    if (std.mem.eql(u8, arg, "help")) return .help;
    return null;
}

fn cmdAnalyze(allocator: std.mem.Allocator, path: []const u8) !void {
    const pe_file = try pe.parseFromFile(allocator, path);
    defer {
        var pf = pe_file;
        allocator.free(pf.data);
        pf.deinit();
    }
    
    const result = vb6.detectVB6(&pe_file);
    
    _ = c.printf("\n=== Binary Analysis ===\n");
    _ = c.printf("File: %s\n", path.ptr);
    _ = c.printf("Image Base: 0x%08X\n", pe_file.getImageBase());
    _ = c.printf("Entry Point: 0x%08X\n", pe_file.getEntryPoint());
    _ = c.printf("Image Size: %zu bytes\n", pe_file.optional_header.size_of_image);
    
    _ = c.printf("\n=== VB6 Detection ===\n");
    _ = c.printf("Is VB: %s\n", if (result.is_vb) "Yes" else "No");
    
    if (result.is_vb) {
        if (result.is_vb6) {
            _ = c.printf("Version: VB6\n");
        } else if (result.is_vb5) {
            _ = c.printf("Version: VB5\n");
        }
        
        _ = c.printf("Binary Type: %s\n", @tagName(result.binary_type).ptr);
        _ = c.printf("Compilation Type: %s\n", @tagName(result.compilation_type).ptr);
        _ = c.printf("Has Forms: %s\n", if (result.has_forms) "Yes" else "No");
        
        if (result.runtime_dll) |dll| {
            _ = c.printf("Runtime DLL: %s\n", dll.ptr);
        }
    }
    
    _ = c.printf("\n");
}

fn cmdSections(allocator: std.mem.Allocator, path: []const u8) !void {
    const pe_file = try pe.parseFromFile(allocator, path);
    defer {
        var pf = pe_file;
        allocator.free(pf.data);
        pf.deinit();
    }
    
    _ = c.printf("\n=== PE Sections ===\n");
    _ = c.printf("%-12s %-12s %-12s %-12s\n", "Name", "VirtAddr", "VirtSize", "RawSize");
    _ = c.printf("--------------------------------------------------\n");
    
    for (pe_file.sections) |section| {
        const name = section.getName();
        _ = c.printf("%-12s 0x%08X   0x%08X   0x%08X\n", 
            name.ptr, 
            section.virtual_address, 
            section.virtual_size, 
            section.size_of_raw_data);
    }
    
    _ = c.printf("\n");
}

fn cmdDisasm(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8) !void {
    // Parse address
    const address = try std.fmt.parseInt(u32, address_str, 0);
    
    const pe_file = try pe.parseFromFile(allocator, path);
    defer {
        var pf = pe_file;
        allocator.free(pf.data);
        pf.deinit();
    }
    
    const image_base = pe_file.getImageBase();
    const rva = address - image_base;
    
    // Get data at RVA
    const section_data = pe_file.rvaToData(rva, 0x1000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Create disassembler
    var disassembler = disasm.Disassembler.init(allocator, section_data, address);
    
    const options = disasm.DisassemblerOptions{
        .start_address = address,
        .end_address = address + @as(u32, @intCast(@min(section_data.len, 0x1000))),
        .max_instructions = 20, // Default to 20 instructions
    };
    
    const instructions = try disassembler.disassemble(options);
    defer disassembler.freeInstructions(instructions);
    
    _ = c.printf("\n=== Disassembly at 0x%08X ===\n", address);
    
    for (instructions) |inst| {
        const formatted = try inst.formatAlloc(allocator);
        defer allocator.free(formatted);
        
        _ = c.printf("%08X: %s\n", inst.address, formatted.ptr);
    }
    
    _ = c.printf("\n");
}

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    
    // Simple manual argument handling for now
    // TODO: Update to proper args API when Zig 0.16 args API is clarified
    const argv = @extern([*][*:0]u8, .{ .name = "environ" }); // Placeholder
    _ = argv;
    _ = allocator;
    
    // For now, just print usage
    printUsage();
}
