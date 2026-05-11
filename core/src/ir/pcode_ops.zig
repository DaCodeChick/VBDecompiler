// P-code operation definitions based on Ghidra P-code specification
// Reference: https://ghidra.re/courses/languages/html/pcoderef.html

const std = @import("std");

/// P-code opcode enumeration
/// Covers the essential Ghidra P-code operations needed for x86 translation
pub const OpCode = enum(u8) {
    // Data movement
    copy, // Copy one varnode to another
    load, // Load from memory
    store, // Store to memory

    // Arithmetic operations
    int_add, // Integer addition
    int_sub, // Integer subtraction
    int_mult, // Integer multiplication (signed/unsigned)
    int_div, // Integer division (unsigned)
    int_sdiv, // Integer division (signed)
    int_rem, // Integer remainder (unsigned)
    int_srem, // Integer remainder (signed)
    int_neg, // Integer negation (two's complement)

    // Logical operations
    int_and, // Bitwise AND
    int_or, // Bitwise OR
    int_xor, // Bitwise XOR
    int_not, // Bitwise NOT (one's complement)
    int_left, // Left shift
    int_right, // Right shift (logical)
    int_sright, // Right shift (arithmetic/signed)

    // Comparison operations (produce boolean 0 or 1)
    int_equal, // Integer equality
    int_notequal, // Integer inequality
    int_less, // Unsigned less than
    int_sless, // Signed less than
    int_lessequal, // Unsigned less than or equal
    int_slessequal, // Signed less than or equal

    // Extension and truncation
    int_zext, // Zero extension
    int_sext, // Sign extension
    subpiece, // Extract a piece (truncate or extract bytes)

    // Boolean operations
    bool_and, // Logical AND
    bool_or, // Logical OR
    bool_xor, // Logical XOR
    bool_negate, // Logical NOT

    // Control flow
    branch, // Unconditional branch
    cbranch, // Conditional branch (input: condition, destination)
    branchind, // Indirect branch
    call, // Direct call
    callind, // Indirect call
    @"return", // Return from call

    // Special
    piece, // Concatenate two varnodes
    popcount, // Count set bits
    lzcount, // Count leading zeros

    pub fn format(self: OpCode, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        try writer.writeAll(@tagName(self));
    }
};

/// Address space types for varnodes
pub const AddressSpace = enum(u8) {
    constant, // Constant value (no actual address)
    register, // Physical register
    unique, // Temporary/unique space (SSA temps)
    ram, // Main memory
    stack, // Stack space (often modeled as negative offsets from stack pointer)

    pub fn format(self: AddressSpace, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        try writer.writeAll(@tagName(self));
    }
};

/// Varnode - represents a storage location or value in P-code
/// Every varnode has: address_space, offset, and size in bytes
pub const Varnode = struct {
    space: AddressSpace,
    offset: u64, // Offset within address space (register number, memory address, etc.)
    size: u32, // Size in bytes

    /// Create a constant varnode
    pub fn constant(value: u64, size: u32) Varnode {
        return .{
            .space = .constant,
            .offset = value,
            .size = size,
        };
    }

    /// Create a register varnode
    pub fn register(offset: u64, size: u32) Varnode {
        return .{
            .space = .register,
            .offset = offset,
            .size = size,
        };
    }

    /// Create a unique/temporary varnode
    pub fn unique(offset: u64, size: u32) Varnode {
        return .{
            .space = .unique,
            .offset = offset,
            .size = size,
        };
    }

    /// Create a RAM/memory varnode
    pub fn ram(address: u64, size: u32) Varnode {
        return .{
            .space = .ram,
            .offset = address,
            .size = size,
        };
    }

    /// Create a stack varnode
    pub fn stack(offset: u64, size: u32) Varnode {
        return .{
            .space = .stack,
            .offset = offset,
            .size = size,
        };
    }

    pub fn format(self: Varnode, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        switch (self.space) {
            .constant => try writer.print("0x{x}:{}", .{ self.offset, self.size }),
            .register => try writer.print("r{x}:{}", .{ self.offset, self.size }),
            .unique => try writer.print("u{x}:{}", .{ self.offset, self.size }),
            .ram => try writer.print("ram[0x{x}]:{}", .{ self.offset, self.size }),
            .stack => try writer.print("stack[0x{x}]:{}", .{ self.offset, self.size }),
        }
    }
};

/// P-code operation
/// Represents a single P-code instruction with opcode and up to 2 inputs + 1 output
pub const PCodeOp = struct {
    opcode: OpCode,
    output: ?Varnode, // Output varnode (null for operations without output)
    input0: ?Varnode, // First input
    input1: ?Varnode, // Second input (if needed)
    seq_num: u64, // Sequence number for ordering within a single instruction

    pub fn format(self: PCodeOp, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        if (self.output) |out| {
            try writer.print("{} = ", .{out});
        }
        try writer.print("{}", .{self.opcode});
        if (self.input0) |in0| {
            try writer.print(" {}", .{in0});
            if (self.input1) |in1| {
                try writer.print(", {}", .{in1});
            }
        }
    }
};

test "varnode creation and formatting" {
    const allocator = std.testing.allocator;

    // Test constant
    const const_vn = Varnode.constant(42, 4);
    try std.testing.expectEqual(AddressSpace.constant, const_vn.space);
    try std.testing.expectEqual(42, const_vn.offset);
    try std.testing.expectEqual(4, const_vn.size);

    var buf: std.ArrayList(u8) = .{ .items = &.{}, .capacity = 0 };
    defer buf.deinit(allocator);
    try buf.writer(allocator).print("{}", .{const_vn});
    try std.testing.expectEqualStrings("0x2a:4", buf.items);

    // Test register
    buf.clearRetainingCapacity();
    const reg_vn = Varnode.register(0, 4);
    try buf.writer(allocator).print("{}", .{reg_vn});
    try std.testing.expectEqualStrings("r0:4", buf.items);

    // Test unique
    buf.clearRetainingCapacity();
    const uniq_vn = Varnode.unique(100, 1);
    try buf.writer(allocator).print("{}", .{uniq_vn});
    try std.testing.expectEqualStrings("u64:1", buf.items);
}

test "pcode op formatting" {
    const allocator = std.testing.allocator;

    var buf: std.ArrayList(u8) = .{ .items = &.{}, .capacity = 0 };
    defer buf.deinit(allocator);

    // Test: r0:4 = int_add r1:4, 0x10:4
    const op = PCodeOp{
        .opcode = .int_add,
        .output = Varnode.register(0, 4),
        .input0 = Varnode.register(1, 4),
        .input1 = Varnode.constant(0x10, 4),
        .seq_num = 0,
    };

    try buf.writer(allocator).print("{}", .{op});
    try std.testing.expectEqualStrings("r0:4 = int_add r1:4, 0x10:4", buf.items);
}
