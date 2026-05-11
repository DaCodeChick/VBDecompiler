// P-code pretty-printer for debugging and visualization
// Formats P-code operations in a human-readable format

const std = @import("std");
const pcode_ops = @import("pcode_ops.zig");
const pcode_function = @import("pcode_function.zig");

const PCodeOp = pcode_ops.PCodeOp;
const PCodeBlock = pcode_function.PCodeBlock;
const PCodeFunction = pcode_function.PCodeFunction;

/// Pretty-printer options
pub const PrinterOptions = struct {
    show_seq_numbers: bool = false, // Show sequence numbers for each operation
    indent: usize = 2, // Number of spaces to indent operations
    show_addresses: bool = true, // Show block addresses
    color: bool = false, // Use ANSI colors (for terminal output)
};

/// P-code pretty-printer
pub const PrettyPrinter = struct {
    allocator: std.mem.Allocator,
    options: PrinterOptions,

    pub fn init(allocator: std.mem.Allocator, options: PrinterOptions) PrettyPrinter {
        return .{
            .allocator = allocator,
            .options = options,
        };
    }

    /// Print a single P-code operation
    pub fn printOp(self: *const PrettyPrinter, writer: anytype, op: *const PCodeOp) !void {
        // Indent
        var i: usize = 0;
        while (i < self.options.indent) : (i += 1) {
            try writer.writeByte(' ');
        }

        // Optional sequence number
        if (self.options.show_seq_numbers) {
            try writer.print("[{d:3}] ", .{op.seq_num});
        }

        // Output varnode (if present)
        if (op.output) |out| {
            if (self.options.color) {
                try writer.writeAll("\x1b[32m"); // Green for output
            }
            try writer.print("{}", .{out});
            if (self.options.color) {
                try writer.writeAll("\x1b[0m"); // Reset
            }
            try writer.writeAll(" = ");
        }

        // Opcode
        if (self.options.color) {
            try writer.writeAll("\x1b[33m"); // Yellow for opcode
        }
        try writer.print("{}", .{op.opcode});
        if (self.options.color) {
            try writer.writeAll("\x1b[0m"); // Reset
        }

        // Input operands
        if (op.input0) |in0| {
            try writer.writeAll(" ");
            if (self.options.color) {
                try writer.writeAll("\x1b[36m"); // Cyan for inputs
            }
            try writer.print("{}", .{in0});
            if (self.options.color) {
                try writer.writeAll("\x1b[0m");
            }

            if (op.input1) |in1| {
                try writer.writeAll(", ");
                if (self.options.color) {
                    try writer.writeAll("\x1b[36m");
                }
                try writer.print("{}", .{in1});
                if (self.options.color) {
                    try writer.writeAll("\x1b[0m");
                }
            }
        }

        try writer.writeByte('\n');
    }

    /// Print a P-code basic block
    pub fn printBlock(self: *const PrettyPrinter, writer: anytype, block: *const PCodeBlock) !void {
        if (self.options.show_addresses) {
            if (self.options.color) {
                try writer.writeAll("\x1b[1;34m"); // Bold blue for block headers
            }
            try writer.print("Block 0x{x:0>8} - 0x{x:0>8}:\n", .{ block.address, block.end_address });
            if (self.options.color) {
                try writer.writeAll("\x1b[0m");
            }
        }

        for (block.ops.items) |*op| {
            try self.printOp(writer, op);
        }
    }

    /// Print a P-code function
    pub fn printFunction(self: *const PrettyPrinter, writer: anytype, func: *const PCodeFunction) !void {
        // Function header
        if (self.options.color) {
            try writer.writeAll("\x1b[1;35m"); // Bold magenta for function headers
        }
        try writer.writeAll("================================================================================\n");
        try writer.print("Function: {s} @ 0x{x:0>8}\n", .{ func.name, func.address });
        try writer.writeAll("================================================================================\n");
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        // Sort blocks by address
        var addresses: std.ArrayList(u32) = .{ .items = &.{}, .capacity = 0 };
        defer addresses.deinit(self.allocator);

        var it = func.blocks.keyIterator();
        while (it.next()) |addr| {
            try addresses.append(self.allocator, addr.*);
        }

        std.mem.sort(u32, addresses.items, {}, comptime std.sort.asc(u32));

        // Print each block
        for (addresses.items) |addr| {
            const block = func.blocks.get(addr).?;
            try self.printBlock(writer, &block);
            try writer.writeByte('\n');
        }
    }

    /// Print P-code operations as a compact listing
    pub fn printCompact(self: *const PrettyPrinter, writer: anytype, ops: []const PCodeOp) !void {
        for (ops) |*op| {
            try self.printOp(writer, op);
        }
    }
};

/// Convenience function to print a function with default options
pub fn printFunction(allocator: std.mem.Allocator, writer: anytype, func: *const PCodeFunction) !void {
    const printer = PrettyPrinter.init(allocator, .{});
    try printer.printFunction(writer, func);
}

/// Convenience function to print a block with default options
pub fn printBlock(allocator: std.mem.Allocator, writer: anytype, block: *const PCodeBlock) !void {
    const printer = PrettyPrinter.init(allocator, .{});
    try printer.printBlock(writer, block);
}

test "pretty print pcode op" {
    const allocator = std.testing.allocator;
    const Varnode = pcode_ops.Varnode;

    var buf: std.ArrayList(u8) = .{ .items = &.{}, .capacity = 0 };
    defer buf.deinit(allocator);

    const printer = PrettyPrinter.init(allocator, .{ .indent = 0 });

    // Test: r0:4 = int_add r1:4, 0x10:4
    const op = PCodeOp{
        .opcode = .int_add,
        .output = Varnode.register(0, 4),
        .input0 = Varnode.register(1, 4),
        .input1 = Varnode.constant(0x10, 4),
        .seq_num = 0,
    };

    try printer.printOp(buf.writer(allocator), &op);
    try std.testing.expect(buf.items.len > 0);
}
