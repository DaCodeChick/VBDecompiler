// VB Decompiler C API implementation
const std = @import("std");
const pe = @import("pe/parser.zig");
const vb6 = @import("vb6/detector.zig");
const vb_structures = @import("vb6/structures.zig");
const disasm = @import("disasm/disassembler.zig");
const Instruction = @import("disasm/instruction.zig").Instruction;
const CFG = @import("analysis/cfg.zig").CFG;
const CFGBuilder = @import("analysis/cfg_builder.zig").CFGBuilder;

// C API types match header definitions
const BinaryType = enum(c_int) {
    unknown = 0,
    exe = 1,
    dll = 2,
    ocx = 3,
};

const CompilationType = enum(c_int) {
    unknown = 0,
    native = 1,
    pcode = 2,
};

const VBVersion = enum(c_int) {
    unknown = 0,
    vb5 = 5,
    vb6 = 6,
};

// Context structure
pub const Context = struct {
    allocator: std.mem.Allocator,
    pe_file: pe.PEFile,
    detection_result: vb6.DetectionResult,
    last_error: ?[]const u8,
    cfg: ?CFG,
    
    pub fn deinit(self: *Context) void {
        if (self.last_error) |err| {
            self.allocator.free(err);
        }
        if (self.cfg) |*cfg_val| {
            var cfg_mut = cfg_val.*;
            cfg_mut.deinit();
        }
        self.allocator.free(self.pe_file.data);
        self.pe_file.deinit();
    }
};

// Global allocator for library - using arena wrapped around page allocator
var arena_instance = std.heap.ArenaAllocator.init(std.heap.page_allocator);
const allocator = arena_instance.allocator();

// Version string
const VERSION_STRING = "VBDecompiler 0.1.0";

// Initialize library
export fn vbdecomp_init() void {
    // Currently no global initialization needed
}

// Cleanup library
export fn vbdecomp_cleanup() void {
    // Currently no global cleanup needed
}

// Get version string
export fn vbdecomp_version() [*:0]const u8 {
    return VERSION_STRING;
}

// Open file
export fn vbdecomp_open(path: [*:0]const u8) ?*Context {
    const path_slice = std.mem.span(path);
    
    const pe_file = pe.parseFromFile(allocator, path_slice) catch |err| {
        std.debug.print("Failed to parse PE file: {}\n", .{err});
        return null;
    };
    
    const detection_result = vb6.detectVB6(&pe_file);
    
    const ctx = allocator.create(Context) catch {
        var pf = pe_file;
        allocator.free(pf.data);
        pf.deinit();
        return null;
    };
    
    ctx.* = Context{
        .allocator = allocator,
        .pe_file = pe_file,
        .detection_result = detection_result,
        .last_error = null,
        .cfg = null,
    };
    
    return ctx;
}

// Open from memory
export fn vbdecomp_open_memory(data: [*]const u8, size: usize) ?*Context {
    const data_slice = data[0..size];
    
    // Make a copy of the data
    const data_copy = allocator.dupe(u8, data_slice) catch return null;
    errdefer allocator.free(data_copy);
    
    const pe_file = pe.PEFile.init(allocator, data_copy) catch {
        allocator.free(data_copy);
        return null;
    };
    
    const detection_result = vb6.detectVB6(&pe_file);
    
    const ctx = allocator.create(Context) catch {
        var pf = pe_file;
        allocator.free(data_copy);
        pf.deinit();
        return null;
    };
    
    ctx.* = Context{
        .allocator = allocator,
        .pe_file = pe_file,
        .detection_result = detection_result,
        .last_error = null,
        .cfg = null,
    };
    
    return ctx;
}

// Close context
export fn vbdecomp_close(ctx: ?*Context) void {
    if (ctx) |c| {
        c.deinit();
        allocator.destroy(c);
    }
}

// Get error message
export fn vbdecomp_get_error(ctx: ?*Context) ?[*:0]const u8 {
    if (ctx) |c| {
        if (c.last_error) |err| {
            // Ensure the error string is null-terminated
            return @ptrCast(err.ptr);
        }
    }
    return null;
}

// C struct for binary info (must match header)
const BinaryInfo = extern struct {
    is_vb: bool,
    vb_version: VBVersion,
    binary_type: BinaryType,
    compilation_type: CompilationType,
    has_forms: bool,
    runtime_dll: ?[*:0]const u8,
    entry_point: u32,
    image_base: u32,
    image_size: usize,
};

// Get binary information
export fn vbdecomp_get_info(ctx: ?*Context, info: ?*BinaryInfo) bool {
    const c = ctx orelse return false;
    const i = info orelse return false;
    
    const vb_ver: VBVersion = if (c.detection_result.is_vb6)
        .vb6
    else if (c.detection_result.is_vb5)
        .vb5
    else
        .unknown;
    
    const bin_type: BinaryType = switch (c.detection_result.binary_type) {
        .exe => .exe,
        .dll => .dll,
        .ocx => .ocx,
        .unknown => .unknown,
    };
    
    const comp_type: CompilationType = switch (c.detection_result.compilation_type) {
        .native => .native,
        .pcode => .pcode,
        .unknown => .unknown,
    };
    
    i.* = BinaryInfo{
        .is_vb = c.detection_result.is_vb,
        .vb_version = vb_ver,
        .binary_type = bin_type,
        .compilation_type = comp_type,
        .has_forms = c.detection_result.has_forms,
        .runtime_dll = if (c.detection_result.runtime_dll) |dll| @ptrCast(dll.ptr) else null,
        .entry_point = c.pe_file.getEntryPoint(),
        .image_base = c.pe_file.getImageBase(),
        .image_size = c.pe_file.optional_header.size_of_image,
    };
    
    return true;
}

// Section info struct
const SectionInfo = extern struct {
    name: [9]u8,
    virtual_address: u32,
    virtual_size: u32,
    raw_size: u32,
    characteristics: u32,
};

// Get section count
export fn vbdecomp_get_section_count(ctx: ?*Context) usize {
    const c = ctx orelse return 0;
    return c.pe_file.sections.len;
}

// Get section
export fn vbdecomp_get_section(ctx: ?*Context, index: usize, section: ?*SectionInfo) bool {
    const c = ctx orelse return false;
    const s = section orelse return false;
    
    if (index >= c.pe_file.sections.len) return false;
    
    const pe_section = &c.pe_file.sections[index];
    
    @memset(&s.name, 0);
    const name_slice = pe_section.getName();
    @memcpy(s.name[0..name_slice.len], name_slice);
    
    s.virtual_address = pe_section.virtual_address;
    s.virtual_size = pe_section.virtual_size;
    s.raw_size = pe_section.size_of_raw_data;
    s.characteristics = pe_section.characteristics;
    
    return true;
}

// Placeholder implementations for not-yet-implemented features
export fn vbdecomp_get_import_count(ctx: ?*Context) usize {
    _ = ctx;
    return 0; // TODO: Implement import parsing
}

export fn vbdecomp_get_import(ctx: ?*Context, index: usize, import: ?*anyopaque) bool {
    _ = ctx;
    _ = index;
    _ = import;
    return false; // TODO: Implement
}

export fn vbdecomp_get_export_count(ctx: ?*Context) usize {
    _ = ctx;
    return 0; // TODO: Implement export parsing
}

export fn vbdecomp_get_export(ctx: ?*Context, index: usize, exp: ?*anyopaque) bool {
    _ = ctx;
    _ = index;
    _ = exp;
    return false; // TODO: Implement
}

export fn vbdecomp_get_string_count(ctx: ?*Context) usize {
    _ = ctx;
    return 0; // TODO: Implement string extraction
}

export fn vbdecomp_get_string(ctx: ?*Context, index: usize, str: ?*anyopaque) bool {
    _ = ctx;
    _ = index;
    _ = str;
    return false; // TODO: Implement
}

export fn vbdecomp_get_function_count(ctx: ?*Context) usize {
    _ = ctx;
    return 0; // TODO: Implement function detection
}

export fn vbdecomp_get_function(ctx: ?*Context, index: usize, func: ?*anyopaque) bool {
    _ = ctx;
    _ = index;
    _ = func;
    return false; // TODO: Implement
}

export fn vbdecomp_disassemble(ctx: ?*Context, address: u32, count: usize) ?[*:0]u8 {
    const c = ctx orelse return null;
    
    // Find the section containing this address
    const rva = address - c.pe_file.getImageBase();
    const section_data = c.pe_file.rvaToData(rva, 0x1000) orelse return null;
    
    // Create disassembler
    var disassembler = disasm.Disassembler.init(allocator, section_data, address);
    
    // Disassemble instructions
    const options = disasm.DisassemblerOptions{
        .start_address = address,
        .end_address = address + @as(u32, @intCast(@min(section_data.len, 0x1000))),
        .max_instructions = if (count == 0) 0 else count,
    };
    
    const instructions = disassembler.disassemble(options) catch return null;
    defer disassembler.freeInstructions(instructions);
    
    // Format instructions to string
    var result: std.ArrayList(u8) = .{ .items = &.{}, .capacity = 0 };
    defer result.deinit(allocator);
    
    for (instructions) |inst| {
        const formatted = inst.formatAlloc(allocator) catch continue;
        defer allocator.free(formatted);
        
        const line = std.fmt.allocPrint(allocator, "{X:0>8}: {s}\n", .{ inst.address, formatted }) catch continue;
        defer allocator.free(line);
        
        result.appendSlice(allocator, line) catch continue;
    }
    
    // Null-terminate and return
    result.append(allocator, 0) catch return null;
    const owned = result.toOwnedSlice(allocator) catch return null;
    return @ptrCast(owned.ptr);
}

export fn vbdecomp_analyze_function(ctx: ?*Context, address: u32) bool {
    const c = ctx orelse return false;
    
    // Get code section
    const image_base = c.pe_file.getImageBase();
    const rva = address - image_base;
    const section_data = c.pe_file.rvaToData(rva, 0x10000) orelse return false;
    
    // Initialize CFG if needed
    if (c.cfg == null) {
        c.cfg = CFG.init(allocator);
    }
    
    // Build CFG for this function
    var builder = CFGBuilder.init(allocator, &c.cfg.?, section_data, address, .{});
    defer builder.deinit();
    
    builder.buildFunction(address) catch return false;
    
    return true;
}

export fn vbdecomp_get_xrefs_to(ctx: ?*Context, address: u32, buffer: ?[*]u32, buffer_size: usize) usize {
    const c = ctx orelse return 0;
    const cfg = c.cfg orelse return 0;
    
    const xrefs = cfg.getXRefsTo(address, allocator) catch return 0;
    defer allocator.free(xrefs);
    
    if (buffer) |buf| {
        const count = @min(xrefs.len, buffer_size);
        for (xrefs[0..count], 0..) |xref, i| {
            buf[i] = xref.from;
        }
    }
    
    return xrefs.len;
}

export fn vbdecomp_get_xrefs_from(ctx: ?*Context, address: u32, buffer: ?[*]u32, buffer_size: usize) usize {
    const c = ctx orelse return 0;
    const cfg = c.cfg orelse return 0;
    
    const xrefs = cfg.getXRefsFrom(address, allocator) catch return 0;
    defer allocator.free(xrefs);
    
    if (buffer) |buf| {
        const count = @min(xrefs.len, buffer_size);
        for (xrefs[0..count], 0..) |xref, i| {
            buf[i] = xref.to;
        }
    }
    
    return xrefs.len;
}

export fn vbdecomp_decompile(ctx: ?*Context, address: u32) ?[*:0]u8 {
    _ = ctx;
    _ = address;
    return null; // TODO: Implement decompiler
}

export fn vbdecomp_free_string(str: ?[*:0]u8) void {
    if (str) |s| {
        const len = std.mem.len(s);
        allocator.free(s[0..len]);
    }
}

export fn vbdecomp_read_bytes(ctx: ?*Context, rva: u32, buffer: [*]u8, size: usize) usize {
    const c = ctx orelse return 0;
    const data = c.pe_file.rvaToData(rva, size) orelse return 0;
    @memcpy(buffer[0..data.len], data);
    return data.len;
}

export fn vbdecomp_rva_to_offset(ctx: ?*Context, rva: u32, offset: ?*u32) bool {
    const c = ctx orelse return false;
    const o = offset orelse return false;
    const file_offset = c.pe_file.rvaToOffset(rva) orelse return false;
    o.* = file_offset;
    return true;
}

test "library API" {
    vbdecomp_init();
    defer vbdecomp_cleanup();
    
    const version = vbdecomp_version();
    try std.testing.expect(std.mem.len(version) > 0);
}
