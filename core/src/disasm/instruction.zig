// x86 instruction structures and types
const std = @import("std");

// x86 instruction prefix flags
pub const Prefix = packed struct {
    lock: bool = false,          // F0: LOCK prefix
    repne: bool = false,         // F2: REPNE/REPNZ prefix
    rep: bool = false,           // F3: REP/REPE/REPZ prefix
    cs_override: bool = false,   // 2E: CS segment override
    ss_override: bool = false,   // 36: SS segment override
    ds_override: bool = false,   // 3E: DS segment override
    es_override: bool = false,   // 26: ES segment override
    fs_override: bool = false,   // 64: FS segment override
    gs_override: bool = false,   // 65: GS segment override
    operand_size: bool = false,  // 66: Operand-size override
    address_size: bool = false,  // 67: Address-size override
    _padding: u5 = 0,
};

// x86 register enumeration
pub const Register = enum(u8) {
    // 8-bit registers
    al = 0,
    cl = 1,
    dl = 2,
    bl = 3,
    ah = 4,
    ch = 5,
    dh = 6,
    bh = 7,
    
    // 16-bit registers (add 8)
    ax = 8,
    cx = 9,
    dx = 10,
    bx = 11,
    sp = 12,
    bp = 13,
    si = 14,
    di = 15,
    
    // 32-bit registers (add 16)
    eax = 16,
    ecx = 17,
    edx = 18,
    ebx = 19,
    esp = 20,
    ebp = 21,
    esi = 22,
    edi = 23,
    
    // Segment registers
    es = 24,
    cs = 25,
    ss = 26,
    ds = 27,
    fs = 28,
    gs = 29,
    
    // Special
    none = 255,
    
    pub fn getName(self: Register) []const u8 {
        return switch (self) {
            .al => "al", .cl => "cl", .dl => "dl", .bl => "bl",
            .ah => "ah", .ch => "ch", .dh => "dh", .bh => "bh",
            .ax => "ax", .cx => "cx", .dx => "dx", .bx => "bx",
            .sp => "sp", .bp => "bp", .si => "si", .di => "di",
            .eax => "eax", .ecx => "ecx", .edx => "edx", .ebx => "ebx",
            .esp => "esp", .ebp => "ebp", .esi => "esi", .edi => "edi",
            .es => "es", .cs => "cs", .ss => "ss", .ds => "ds",
            .fs => "fs", .gs => "gs",
            .none => "none",
        };
    }
    
    pub fn getSize(self: Register) u8 {
        return switch (@intFromEnum(self)) {
            0...7 => 1,    // 8-bit
            8...15 => 2,   // 16-bit
            16...23 => 4,  // 32-bit
            24...29 => 2,  // Segment registers (16-bit)
            else => 0,
        };
    }
};

// Operand types
pub const OperandType = enum {
    none,
    register,
    immediate,
    memory,
    relative,
};

// Memory operand addressing
pub const MemoryOperand = struct {
    segment: Register = .none,
    base: Register = .none,
    index: Register = .none,
    scale: u8 = 1,  // 1, 2, 4, 8
    displacement: i32 = 0,
    has_displacement: bool = false,
};

// Instruction operand
pub const Operand = union(OperandType) {
    none: void,
    register: Register,
    immediate: u32,
    memory: MemoryOperand,
    relative: i32,
    
    pub fn format(self: *const Operand, writer: anytype) !void {
        switch (self.*) {
            .none => {},
            .register => |reg| try writer.print("{s}", .{reg.getName()}),
            .immediate => |imm| try writer.print("0x{x}", .{imm}),
            .relative => |rel| try writer.print("0x{x}", .{rel}),
            .memory => |mem| {
                try writer.writeAll("[");
                if (mem.segment != .none) {
                    try writer.print("{s}:", .{mem.segment.getName()});
                }
                
                var needs_plus = false;
                if (mem.base != .none) {
                    try writer.print("{s}", .{mem.base.getName()});
                    needs_plus = true;
                }
                
                if (mem.index != .none) {
                    if (needs_plus) try writer.writeAll("+");
                    try writer.print("{s}", .{mem.index.getName()});
                    if (mem.scale > 1) {
                        try writer.print("*{d}", .{mem.scale});
                    }
                    needs_plus = true;
                }
                
                if (mem.has_displacement) {
                    if (mem.displacement >= 0) {
                        if (needs_plus) try writer.writeAll("+");
                        try writer.print("0x{x}", .{mem.displacement});
                    } else {
                        try writer.print("-0x{x}", .{-mem.displacement});
                    }
                }
                
                try writer.writeAll("]");
            },
        }
    }
    
    pub fn formatAlloc(self: *const Operand, allocator: std.mem.Allocator) ![]const u8 {
        return switch (self.*) {
            .none => try allocator.dupe(u8, ""),
            .register => |reg| try allocator.dupe(u8, reg.getName()),
            .immediate => |imm| try std.fmt.allocPrint(allocator, "0x{x}", .{imm}),
            .relative => |rel| try std.fmt.allocPrint(allocator, "0x{x}", .{rel}),
            .memory => |mem| blk: {
                var result: std.ArrayList(u8) = .{ .items = &.{}, .capacity = 0 };
                errdefer result.deinit(allocator);
                
                try result.append(allocator, '[');
                
                if (mem.segment != .none) {
                    try result.appendSlice(allocator, mem.segment.getName());
                    try result.append(allocator, ':');
                }
                
                var needs_plus = false;
                if (mem.base != .none) {
                    try result.appendSlice(allocator, mem.base.getName());
                    needs_plus = true;
                }
                
                if (mem.index != .none) {
                    if (needs_plus) try result.append(allocator, '+');
                    try result.appendSlice(allocator, mem.index.getName());
                    if (mem.scale > 1) {
                        const scale_str = try std.fmt.allocPrint(allocator, "*{d}", .{mem.scale});
                        defer allocator.free(scale_str);
                        try result.appendSlice(allocator, scale_str);
                    }
                    needs_plus = true;
                }
                
                if (mem.has_displacement) {
                    if (mem.displacement >= 0) {
                        if (needs_plus) try result.append(allocator, '+');
                        const disp_str = try std.fmt.allocPrint(allocator, "0x{x}", .{mem.displacement});
                        defer allocator.free(disp_str);
                        try result.appendSlice(allocator, disp_str);
                    } else {
                        const disp_str = try std.fmt.allocPrint(allocator, "-0x{x}", .{-mem.displacement});
                        defer allocator.free(disp_str);
                        try result.appendSlice(allocator, disp_str);
                    }
                }
                
                try result.append(allocator, ']');
                break :blk try result.toOwnedSlice(allocator);
            },
        };
    }
};

// Instruction mnemonic
pub const Mnemonic = enum {
    // Data movement
    mov, push, pop, lea, xchg,
    
    // Arithmetic
    add, sub, inc, dec, mul, imul, div, idiv,
    neg, adc, sbb,
    
    // Logic
    and_, or_, xor, not, test_,
    
    // Shift/Rotate
    shl, shr, sal, sar, rol, ror, rcl, rcr,
    
    // Control flow
    jmp, je, jne, jz, jnz, ja, jae, jb, jbe,
    jg, jge, jl, jle, js, jns, jo, jno,
    call, ret, retn, retf,
    
    // Comparison
    cmp,
    
    // String operations
    movs, lods, stos, scas, cmps,
    rep, repe, repne, repz, repnz,
    
    // Stack
    enter, leave,
    
    // Misc
    nop, int, hlt, leave_,
    
    // Special/Invalid
    invalid,
    
    pub fn getName(self: Mnemonic) []const u8 {
        return switch (self) {
            .and_ => "and",
            .or_ => "or",
            .test_ => "test",
            .leave_ => "leave",
            else => @tagName(self),
        };
    }
};

// Complete instruction
pub const Instruction = struct {
    address: u32,
    length: u8,
    bytes: [15]u8 = [_]u8{0} ** 15,  // Max x86 instruction is 15 bytes
    prefix: Prefix = .{},
    mnemonic: Mnemonic,
    operands: [3]Operand = [_]Operand{.none} ** 3,
    
    pub fn format(self: *const Instruction, writer: anytype) !void {
        // Write address
        try writer.print("{x:0>8}  ", .{self.address});
        
        // Write bytes
        var i: usize = 0;
        while (i < self.length) : (i += 1) {
            try writer.print("{x:0>2}", .{self.bytes[i]});
        }
        // Pad to 16 chars (8 bytes)
        while (i < 8) : (i += 1) {
            try writer.writeAll("  ");
        }
        
        try writer.writeAll("  ");
        
        // Write mnemonic
        try writer.print("{s:<8}", .{self.mnemonic.getName()});
        
        // Write operands
        var first = true;
        for (self.operands) |*op| {
            if (op.* == .none) break;
            
            if (!first) {
                try writer.writeAll(", ");
            }
            first = false;
            
            try op.format(writer);
        }
        
        try writer.writeAll("\n");
    }
    
    /// Format instruction to allocated string
    pub fn formatAlloc(self: *const Instruction, allocator: std.mem.Allocator) ![]const u8 {
        // Format mnemonic
        const mnem = self.mnemonic.getName();
        
        // Format operands
        var operand_strs: [3][]const u8 = .{ "", "", "" };
        var operand_count: usize = 0;
        for (self.operands, 0..) |op, i| {
            if (op == .none) break;
            operand_strs[i] = try op.formatAlloc(allocator);
            operand_count += 1;
        }
        defer {
            for (operand_strs[0..operand_count]) |s| {
                allocator.free(s);
            }
        }
        
        // Build final string
        var result: std.ArrayList(u8) = .{ .items = &.{}, .capacity = 0 };
        errdefer result.deinit(allocator);
        
        // Just return mnemonic + operands for simplicity
        try result.appendSlice(allocator, mnem);
        if (operand_count > 0) {
            try result.append(allocator, ' ');
            for (operand_strs[0..operand_count], 0..) |s, i| {
                if (i > 0) {
                    try result.appendSlice(allocator, ", ");
                }
                try result.appendSlice(allocator, s);
            }
        }
        
        return result.toOwnedSlice(allocator);
    }
};

// ModR/M byte structure
pub const ModRM = packed struct {
    rm: u3,
    reg: u3,
    mod: u2,
};

// SIB byte structure
pub const SIB = packed struct {
    base: u3,
    index: u3,
    scale: u2,
};

test "instruction formatting" {
    const allocator = std.testing.allocator;
    var buffer: std.ArrayList(u8) = .{ .items = &.{}, .capacity = 0 };
    defer buffer.deinit(allocator);
    
    const inst = Instruction{
        .address = 0x401000,
        .length = 5,
        .bytes = [_]u8{ 0xB8, 0x01, 0x00, 0x00, 0x00 } ++ [_]u8{0} ** 10,
        .mnemonic = .mov,
        .operands = .{
            .{ .register = .eax },
            .{ .immediate = 0x1 },
            .none,
        },
    };
    
    // Use formatAlloc instead since writer API changed
    const result = try inst.formatAlloc(allocator);
    defer allocator.free(result);
    
    try std.testing.expect(std.mem.indexOf(u8, result, "mov") != null);
    try std.testing.expect(std.mem.indexOf(u8, result, "eax") != null);
}
