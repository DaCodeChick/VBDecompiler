// VB Decompiler CLI tool
const std = @import("std");
const pe = @import("pe/parser.zig");
const vb6 = @import("vb6/detector.zig");
const disasm = @import("disasm/disassembler.zig");
const Instruction = @import("disasm/instruction.zig").Instruction;
const CFG = @import("analysis/cfg.zig").CFG;
const CFGBuilder = @import("analysis/cfg_builder.zig").CFGBuilder;
const cfg_export = @import("analysis/cfg_export.zig");
const PCodeBuilder = @import("ir/pcode_function.zig").PCodeBuilder;
const pcode_printer = @import("ir/pcode_printer.zig");
const ReachingDefsAnalyzer = @import("analysis/reaching_defs.zig").ReachingDefsAnalyzer;
const LivenessAnalyzer = @import("analysis/liveness.zig").LivenessAnalyzer;
const DominatorTreeBuilder = @import("analysis/dominators.zig").DominatorTreeBuilder;
const dataflow_printer = @import("analysis/dataflow_printer.zig");
const SSAConverter = @import("ir/ssa.zig").SSAConverter;
const ssa_printer = @import("ir/ssa_printer.zig");

const c = @cImport({
    @cInclude("stdio.h");
});

const Command = enum {
    analyze,
    sections,
    disasm,
    cfg,
    xrefs,
    blocks,
    dot,
    pcode,
    dataflow,
    ssa,
    help,
};

fn printUsage() void {
    _ = c.printf("VBDecompiler CLI v0.1.0\n");
    _ = c.printf("Usage: vbdecomp <command> [options] <file>\n\n");
    _ = c.printf("Commands:\n");
    _ = c.printf("  analyze <file>              - Analyze VB6 binary\n");
    _ = c.printf("  sections <file>             - List PE sections\n");
    _ = c.printf("  disasm <file> <address>     - Disassemble at address (hex)\n");
    _ = c.printf("  cfg <file> <address>        - Analyze control flow at address (hex)\n");
    _ = c.printf("  xrefs <file> <address>      - Show cross-references to/from address (hex)\n");
    _ = c.printf("  blocks <file> <address>     - List basic blocks in function (hex)\n");
    _ = c.printf("  dot <file> <address> [out]  - Export CFG to DOT format (Graphviz)\n");
    _ = c.printf("  pcode <file> <address>      - Generate P-code IR for function\n");
    _ = c.printf("  dataflow <file> <address>   - Perform data flow analysis on function\n");
    _ = c.printf("  ssa <file> <address>        - Convert function to SSA form\n");
    _ = c.printf("  help                        - Show this help\n\n");
    _ = c.printf("Examples:\n");
    _ = c.printf("  vbdecomp analyze app.exe\n");
    _ = c.printf("  vbdecomp sections app.exe\n");
    _ = c.printf("  vbdecomp disasm app.exe 0x401000\n");
    _ = c.printf("  vbdecomp cfg app.exe 0x401000\n");
    _ = c.printf("  vbdecomp xrefs app.exe 0x401000\n");
    _ = c.printf("  vbdecomp blocks app.exe 0x401000\n");
    _ = c.printf("  vbdecomp dot app.exe 0x401000 cfg.dot\n");
    _ = c.printf("  vbdecomp pcode app.exe 0x401000\n");
    _ = c.printf("  vbdecomp dataflow app.exe 0x401000\n");
    _ = c.printf("  vbdecomp ssa app.exe 0x401000\n");
}

fn parseCommand(arg: []const u8) ?Command {
    if (std.mem.eql(u8, arg, "analyze")) return .analyze;
    if (std.mem.eql(u8, arg, "sections")) return .sections;
    if (std.mem.eql(u8, arg, "disasm")) return .disasm;
    if (std.mem.eql(u8, arg, "cfg")) return .cfg;
    if (std.mem.eql(u8, arg, "xrefs")) return .xrefs;
    if (std.mem.eql(u8, arg, "blocks")) return .blocks;
    if (std.mem.eql(u8, arg, "dot")) return .dot;
    if (std.mem.eql(u8, arg, "pcode")) return .pcode;
    if (std.mem.eql(u8, arg, "dataflow")) return .dataflow;
    if (std.mem.eql(u8, arg, "ssa")) return .ssa;
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
    _ = c.printf("Is VB: %s\n", if (result.is_vb) "Yes".ptr else "No".ptr);
    
    if (result.is_vb) {
        if (result.is_vb6) {
            _ = c.printf("Version: VB6\n");
        } else if (result.is_vb5) {
            _ = c.printf("Version: VB5\n");
        }
        
        _ = c.printf("Binary Type: %s\n", @tagName(result.binary_type).ptr);
        _ = c.printf("Compilation Type: %s\n", @tagName(result.compilation_type).ptr);
        _ = c.printf("Has Forms: %s\n", if (result.has_forms) "Yes".ptr else "No".ptr);
        
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

fn cmdCfg(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8) !void {
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
    
    // Get code section
    const section_data = pe_file.rvaToData(rva, 0x10000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Build CFG
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    var builder = CFGBuilder.init(allocator, &cfg, section_data, address, .{});
    defer builder.deinit();
    
    try builder.buildFunction(address);
    
    // Display results
    _ = c.printf("\n=== Control Flow Analysis at 0x%08X ===\n", address);
    
    const func = cfg.functions.get(address);
    if (func) |f| {
        _ = c.printf("Function: 0x%08X\n", f.address);
        _ = c.printf("Basic Blocks: %zu\n", f.blocks.len);
        _ = c.printf("Calling Convention: %s\n", @tagName(f.convention).ptr);
        
        var total_instructions: usize = 0;
        for (f.blocks) |bb_addr| {
            if (cfg.blocks.get(bb_addr)) |bb| {
                total_instructions += bb.instructions.len;
            }
        }
        _ = c.printf("Total Instructions: %zu\n", total_instructions);
        
        // Show cross-references
        const xrefs_to = try cfg.getXRefsTo(address, allocator);
        defer allocator.free(xrefs_to);
        
        const xrefs_from = try cfg.getXRefsFrom(address, allocator);
        defer allocator.free(xrefs_from);
        
        _ = c.printf("XRefs To: %zu\n", xrefs_to.len);
        _ = c.printf("XRefs From: %zu\n", xrefs_from.len);
    } else {
        _ = c.printf("Error: Failed to build function at address\n");
    }
    
    _ = c.printf("\n");
}

fn cmdXRefs(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8) !void {
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
    
    // Get code section
    const section_data = pe_file.rvaToData(rva, 0x10000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Build CFG
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    var builder = CFGBuilder.init(allocator, &cfg, section_data, address, .{});
    defer builder.deinit();
    
    try builder.buildFunction(address);
    
    // Display cross-references TO this address
    const xrefs_to = try cfg.getXRefsTo(address, allocator);
    defer allocator.free(xrefs_to);
    
    _ = c.printf("\n=== Cross-References TO 0x%08X ===\n", address);
    if (xrefs_to.len == 0) {
        _ = c.printf("(none)\n");
    } else {
        for (xrefs_to) |xref| {
            _ = c.printf("0x%08X -> 0x%08X (%s)\n", 
                xref.from, xref.to, @tagName(xref.type).ptr);
        }
    }
    
    // Display cross-references FROM this address
    const xrefs_from = try cfg.getXRefsFrom(address, allocator);
    defer allocator.free(xrefs_from);
    
    _ = c.printf("\n=== Cross-References FROM 0x%08X ===\n", address);
    if (xrefs_from.len == 0) {
        _ = c.printf("(none)\n");
    } else {
        for (xrefs_from) |xref| {
            _ = c.printf("0x%08X -> 0x%08X (%s)\n", 
                xref.from, xref.to, @tagName(xref.type).ptr);
        }
    }
    
    _ = c.printf("\n");
}

fn cmdBlocks(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8) !void {
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
    
    // Get code section
    const section_data = pe_file.rvaToData(rva, 0x10000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Build CFG
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    var builder = CFGBuilder.init(allocator, &cfg, section_data, address, .{});
    defer builder.deinit();
    
    try builder.buildFunction(address);
    
    // Display basic blocks
    _ = c.printf("\n=== Basic Blocks in Function at 0x%08X ===\n", address);
    
    const func = cfg.functions.get(address);
    if (func) |f| {
        _ = c.printf("%-12s %-12s %-12s %-12s\n", "Address".ptr, "End".ptr, "Instructions".ptr, "Successors".ptr);
        _ = c.printf("-------------------------------------------------------\n");
        
        for (f.blocks) |bb_addr| {
            if (cfg.blocks.get(bb_addr)) |bb| {
                _ = c.printf("0x%08X   0x%08X   %-12zu %-12zu\n", 
                    bb.start_address, 
                    bb.end_address,
                    bb.instructions.len,
                    bb.successors.len);
            }
        }
    } else {
        _ = c.printf("Error: Failed to build function at address\n");
    }
    
    _ = c.printf("\n");
}

fn cmdDot(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8, output_path: ?[]const u8) !void {
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
    
    // Get code section
    const section_data = pe_file.rvaToData(rva, 0x10000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Build CFG
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    var builder = CFGBuilder.init(allocator, &cfg, section_data, address, .{});
    defer builder.deinit();
    
    try builder.buildFunction(address);
    
    // Create Io instance for file operations
    var threaded = std.Io.Threaded.init(allocator, .{});
    const io = threaded.io();
    
    // Export to DOT
    if (output_path) |out_path| {
        // Open file for writing
        const file = try std.Io.Dir.createFile(std.Io.Dir.cwd(), io, out_path, .{});
        defer file.close(io);
        
        // Create writer with buffer
        var write_buffer: [8192]u8 = undefined;
        var writer = file.writer(io, &write_buffer);
        
        // Export - use the interface field to get std.Io.Writer
        try cfg_export.exportFunctionToDot(allocator, &cfg, address, &writer.interface);
        
        // Flush any remaining buffered data
        try writer.flush();
        
        _ = c.printf("CFG exported to %s\n", out_path.ptr);
    } else {
        // Write to stdout
        const stdout_file = std.Io.File.stdout();
        var write_buffer: [8192]u8 = undefined;
        var writer = stdout_file.writer(io, &write_buffer);
        
        try cfg_export.exportFunctionToDot(allocator, &cfg, address, &writer.interface);
        try writer.flush();
    }
}

fn cmdPCode(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8) !void {
    const address = try std.fmt.parseInt(u32, address_str, 0);
    
    const pe_file = try pe.parseFromFile(allocator, path);
    defer {
        var pf = pe_file;
        allocator.free(pf.data);
        pf.deinit();
    }
    
    const image_base = pe_file.getImageBase();
    const rva = address - image_base;
    
    // Get code section
    const section_data = pe_file.rvaToData(rva, 0x10000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Disassemble to get instructions
    var disassembler = disasm.Disassembler.init(allocator, section_data, address);
    
    const instructions = try disassembler.disassemble(.{
        .start_address = address,
        .end_address = address + @as(u32, @intCast(@min(section_data.len, 0x10000))),
        .max_instructions = 100,
    });
    defer allocator.free(instructions);
    
    // Build CFG to understand function structure
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    var cfg_builder = CFGBuilder.init(allocator, &cfg, section_data, address, .{});
    defer cfg_builder.deinit();
    
    try cfg_builder.buildFunction(address);
    
    // Build P-code representation
    var pcode_builder = PCodeBuilder.init(allocator);
    defer pcode_builder.deinit();
    
    var pcode_func = try pcode_builder.buildFunction(&cfg, address, instructions);
    defer pcode_func.deinit();
    
    // Print P-code using pretty-printer
    _ = c.printf("\n");
    
    // Create Io instance for stdout
    var threaded = std.Io.Threaded.init(allocator, .{});
    const io = threaded.io();
    
    const stdout_file = std.Io.File.stdout();
    var write_buffer: [8192]u8 = undefined;
    var writer = stdout_file.writer(io, &write_buffer);
    
    const printer = pcode_printer.PrettyPrinter.init(allocator, .{
        .show_seq_numbers = false,
        .color = true,
    });
    
    try printer.printFunction(&writer.interface, &pcode_func);
    try writer.flush();
    
    _ = c.printf("\n");
}

fn cmdDataFlow(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8) !void {
    const address = try std.fmt.parseInt(u32, address_str, 0);
    
    const pe_file = try pe.parseFromFile(allocator, path);
    defer {
        var pf = pe_file;
        allocator.free(pf.data);
        pf.deinit();
    }
    
    const image_base = pe_file.getImageBase();
    const rva = address - image_base;
    
    // Get code section
    const section_data = pe_file.rvaToData(rva, 0x10000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Disassemble to get instructions
    var disassembler = disasm.Disassembler.init(allocator, section_data, address);
    
    const instructions = try disassembler.disassemble(.{
        .start_address = address,
        .end_address = address + @as(u32, @intCast(@min(section_data.len, 0x10000))),
        .max_instructions = 100,
    });
    defer allocator.free(instructions);
    
    // Build CFG
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    var cfg_builder = CFGBuilder.init(allocator, &cfg, section_data, address, .{});
    defer cfg_builder.deinit();
    
    try cfg_builder.buildFunction(address);
    
    // Build P-code representation
    var pcode_builder = PCodeBuilder.init(allocator);
    defer pcode_builder.deinit();
    
    var pcode_func = try pcode_builder.buildFunction(&cfg, address, instructions);
    defer pcode_func.deinit();
    
    _ = c.printf("\n=== Data Flow Analysis ===\n\n");
    
    // Perform reaching definitions analysis
    var reach_analyzer = ReachingDefsAnalyzer.init(allocator, &cfg, &pcode_func);
    var reach_analysis = try reach_analyzer.analyze();
    defer reach_analysis.deinit();
    
    // Perform liveness analysis
    var live_analyzer = LivenessAnalyzer.init(allocator, &cfg, &pcode_func);
    var live_analysis = try live_analyzer.analyze();
    defer live_analysis.deinit();
    
    // Build dominator tree
    var dom_builder = DominatorTreeBuilder.init(allocator, &cfg, address);
    var dom_tree = try dom_builder.build();
    defer dom_tree.deinit();
    
    var dom_frontier = try dom_builder.buildDominanceFrontier(&dom_tree);
    defer dom_frontier.deinit();
    
    // Print results
    var threaded = std.Io.Threaded.init(allocator, .{});
    const io = threaded.io();
    
    const stdout_file = std.Io.File.stdout();
    var write_buffer: [8192]u8 = undefined;
    var writer = stdout_file.writer(io, &write_buffer);
    
    const printer = dataflow_printer.DataFlowPrinter.init(allocator, .{
        .color = true,
        .show_reaching_defs = true,
        .show_liveness = true,
        .show_dominators = true,
        .show_use_def_chains = true,
    });
    
    try printer.printAll(&writer.interface, &reach_analysis, &live_analysis, &dom_tree, &dom_frontier, &pcode_func);
    try writer.flush();
    
    _ = c.printf("\n");
}

fn cmdSSA(allocator: std.mem.Allocator, path: []const u8, address_str: []const u8) !void {
    const address = try std.fmt.parseInt(u32, address_str, 0);
    
    const pe_file = try pe.parseFromFile(allocator, path);
    defer {
        var pf = pe_file;
        allocator.free(pf.data);
        pf.deinit();
    }
    
    const image_base = pe_file.getImageBase();
    const rva = address - image_base;
    
    // Get code section
    const section_data = pe_file.rvaToData(rva, 0x10000) orelse {
        _ = c.printf("Error: Address 0x%08X is not mapped in any section\n", address);
        return error.InvalidAddress;
    };
    
    // Disassemble
    var disassembler = disasm.Disassembler.init(allocator, section_data, address);
    const instructions = try disassembler.disassemble(.{
        .start_address = address,
        .end_address = address + @as(u32, @intCast(@min(section_data.len, 0x10000))),
        .max_instructions = 100,
    });
    defer allocator.free(instructions);
    
    // Build CFG
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    var cfg_builder = CFGBuilder.init(allocator, &cfg, section_data, address, .{});
    defer cfg_builder.deinit();
    
    try cfg_builder.buildFunction(address);
    
    // Build P-code
    var pcode_builder = PCodeBuilder.init(allocator);
    defer pcode_builder.deinit();
    
    var pcode_func = try pcode_builder.buildFunction(&cfg, address, instructions);
    defer pcode_func.deinit();
    
    // Build dominator tree
    var dom_builder = DominatorTreeBuilder.init(allocator, &cfg, address);
    var dom_tree = try dom_builder.build();
    defer dom_tree.deinit();
    
    var dom_frontier = try dom_builder.buildDominanceFrontier(&dom_tree);
    defer dom_frontier.deinit();
    
    // Convert to SSA
    var ssa_converter = SSAConverter.init(allocator, &cfg, &pcode_func, &dom_tree, &dom_frontier);
    defer ssa_converter.deinit();
    
    var ssa_func = try ssa_converter.convert();
    defer ssa_func.deinit();
    
    // Print SSA
    var threaded = std.Io.Threaded.init(allocator, .{});
    const io = threaded.io();
    
    const stdout_file = std.Io.File.stdout();
    var write_buffer: [8192]u8 = undefined;
    var writer = stdout_file.writer(io, &write_buffer);
    
    const printer = ssa_printer.SSAPrinter.init(allocator, .{
        .color = true,
        .show_addresses = true,
        .indent = 2,
    });
    
    try printer.printFunction(&writer.interface, &ssa_func);
    try printer.printStats(&writer.interface, &ssa_func);
    try writer.flush();
    
    _ = c.printf("\n");
}

// Conditionally export main only when building an executable
pub export fn main(argc: c_int, argv: [*][*:0]u8) c_int {
    // Setup arena allocator
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    
    // Convert argc/argv to slice
    const args_count: usize = @intCast(argc);
    const argv_slice = argv[0..args_count];
    
    // Convert to []const u8 slices
    var args_list: std.ArrayList([]const u8) = .{ .items = &.{}, .capacity = 0 };
    defer args_list.deinit(allocator);
    
    for (argv_slice) |arg| {
        args_list.append(allocator, std.mem.span(arg)) catch return 1;
    }
    
    const args = args_list.items;
    
    // Need at least a command
    if (args.len < 2) {
        printUsage();
        return 0;
    }
    
    const cmd = parseCommand(args[1]) orelse {
        _ = c.printf("Error: Unknown command '%s'\n", args[1].ptr);
        printUsage();
        return 1;
    };
    
    switch (cmd) {
        .help => {
            printUsage();
        },
        .analyze => {
            if (args.len < 3) {
                _ = c.printf("Error: analyze command requires a file path\n");
                return 1;
            }
            cmdAnalyze(allocator, args[2]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .sections => {
            if (args.len < 3) {
                _ = c.printf("Error: sections command requires a file path\n");
                return 1;
            }
            cmdSections(allocator, args[2]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .disasm => {
            if (args.len < 4) {
                _ = c.printf("Error: disasm command requires a file path and address\n");
                return 1;
            }
            cmdDisasm(allocator, args[2], args[3]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .cfg => {
            if (args.len < 4) {
                _ = c.printf("Error: cfg command requires a file path and address\n");
                return 1;
            }
            cmdCfg(allocator, args[2], args[3]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .xrefs => {
            if (args.len < 4) {
                _ = c.printf("Error: xrefs command requires a file path and address\n");
                return 1;
            }
            cmdXRefs(allocator, args[2], args[3]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .blocks => {
            if (args.len < 4) {
                _ = c.printf("Error: blocks command requires a file path and address\n");
                return 1;
            }
            cmdBlocks(allocator, args[2], args[3]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .dot => {
            if (args.len < 4) {
                _ = c.printf("Error: dot command requires a file path and address\n");
                return 1;
            }
            const output_file = if (args.len >= 5) args[4] else null;
            cmdDot(allocator, args[2], args[3], output_file) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .pcode => {
            if (args.len < 4) {
                _ = c.printf("Error: pcode command requires a file path and address\n");
                return 1;
            }
            cmdPCode(allocator, args[2], args[3]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .dataflow => {
            if (args.len < 4) {
                _ = c.printf("Error: dataflow command requires a file path and address\n");
                return 1;
            }
            cmdDataFlow(allocator, args[2], args[3]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
        .ssa => {
            if (args.len < 4) {
                _ = c.printf("Error: ssa command requires a file path and address\n");
                return 1;
            }
            cmdSSA(allocator, args[2], args[3]) catch |err| {
                _ = c.printf("Error: %s\n", @errorName(err).ptr);
                return 1;
            };
        },
    }
    
    return 0;
}
